#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <stdio.h>
#include <tcinit/tcinit.h>
#include <tc/tc_startup.h>
#include <tc/emh.h>
#include <tc/tc_util.h>
#include <tccore/item.h>
#include <tccore/aom.h>
#include <tccore/aom_prop.h>
#include <ics/ics.h>
#include <ics/ics2.h>
#include <string>
#include <iostream>
#include <string.h>
#include <ps/ps.h>
#include <bom/bom.h>
#include <tc/folder.h>
#include <epm/epm.h>
#include <time.h>
#include <errno.h>
#include <iostream>

int tc_login(int argc, char* argv[]) {
    if (ITK_ok != ITK_init_from_cpp(argc, argv))
    {
        fprintf(stderr, "ERROR: Inicializace ITK selhala\n");
        return 1;
    }

    ITK_initialize_text_services(0);

    // Login
    int ReturnCode = TC_init_module("infodba", "infodba", "");
    if (ReturnCode != ITK_ok)
    {
        char* Message;
        EMH_ask_error_text(ReturnCode, &Message);
        fprintf(stderr, "ERROR: %s\n", Message);
        MEM_free(Message);
    }

    printf("\nLogin OK\n\n");

    return 0;
}

int change_ownership(tag_t tag1, tag_t tag2, const std::string& user, const std::string& userGroup) {

    //mìní ownership objektu, pøedpokládá se zmìna položky i revize najednou
    AOM_unlock(tag1);
    AOM_unlock(tag2);

    tag_t userTag;
    tag_t userGroupTag;

    SA_find_user2(user.c_str(), &userTag);
    SA_find_group(userGroup.c_str(), &userGroupTag);

    AOM_set_ownership(tag1, userTag, userGroupTag);
    AOM_set_ownership(tag2, userTag, userGroupTag);

    AOM_lock(tag1);
    AOM_lock(tag2);

    return 0;
}

tag_t create_item(const std::string& itemId, const std::string& itemName, const std::string& itemType, const std::string& itemDesc, const std::string& user, const std::string& userGroup) {
    tag_t Item = NULLTAG;
    tag_t Rev = NULLTAG;
    int ReturnCode = ITEM_create_item(itemId.c_str(), itemName.c_str(), itemType.c_str(), 0, &Item, &Rev);
    if (ReturnCode == ITK_ok)
        {
        ITEM_set_description(Item, itemDesc.c_str());
        ITEM_set_rev_description(Rev, itemDesc.c_str());

        ITEM_save_item(Item);
        ITEM_save_rev(Rev);

        //ownership se mìní pøímo ve funkci create item, v pøípadì jiného použít je potøeba tuto funkci upravit
        change_ownership(Item, Rev, user, userGroup);
        

        char* Id = new char[ITEM_id_size_c + 1];
        char* Name = new char[ITEM_name_size_c + 1];

        FL_user_update_newstuff_folder(Item);
        ITEM_ask_id2(Item, &Id);
        ITEM_ask_name2(Item, &Name);

        printf("\nVytvoreni polozky: %s-%s\n", Id, Name);
    }
    else
        fprintf(stderr, "ERROR: Vytvoøení položky selhalo\n");

    //returns ItemRevision Tag
    return Rev;
}

tag_t* find_item(const std::string& InputAttrValues) {
    // Vyhledání položek
    const char* AttrNames[1] = { ITEM_ITEM_ID_PROP };
    const char* AttrValues[1] = { InputAttrValues.c_str() };
    int ItemsCount = 0;
    //tag_t* Items = NULLTAG;
    tag_t* Items;
    ITEM_find_items_by_key_attributes(1, AttrNames, AttrValues, &ItemsCount, &Items);


    /* old
    // Výpis položek

    char Id[ITEM_id_size_c + 1];
    char Name[ITEM_name_size_c + 1];
    for (int i = 0; i < ItemsCount; i++)
    {
        //ITEM_find_item
        ITEM_ask_id(Items[i], Id);
        ITEM_ask_name(Items[i], Name);
        printf("%s %s\n", Id, Name);
    }
    */

    // new
    // Výpis položek
    /*
    char* Id = new char[ITEM_id_size_c + 1];
    char* Name = new char[ITEM_name_size_c + 1];
    for (int i = 0; i < ItemsCount; i++)
    {
        //ITEM_find_item
        ITEM_ask_id2(Items[i], &Id);
        ITEM_ask_name2(Items[i], &Name);

        printf("%s %s\n", Id, Name);
    }
    */

    //vrátí tagy revize
    return Items;
}

std::vector<std::string> splitString(const std::string& line, char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream tokenStream(line);
    std::string token;

    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

std::vector<std::vector<std::string>> readCSV(const std::string& filename) {
    // funkce na nahrání csv souboru do "2D listu" 
    std::vector<std::vector<std::string>> data;
    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
        std::vector<std::string> row = splitString(line, ';');
        data.push_back(row);
    }

    file.close();

    return data;
}

int main(int argc, char* argv[]) {

    std::string itemType;
    std::string user;
    std::string userGroup;
    std::string csvPath;

    std::cout << "\nNapiste druh polozky, ktera se ma vytvorit.\n";
    std::cin >> itemType;
    std::cout << "\nNapiste ciloveho vlastnika polozky.\n";
    std::cin >> user;
    std::cout << "\nNapiste skupinu.\n";
    std::cin >> userGroup;
    std::cout << "\nPresunte csv soubor do konzole.\n";
    std::cin >> csvPath;

    //std::vector<std::vector<std::string>> csvData = readCSV("C:\\Users\\infodba\\Desktop\\BOM Test\\BOM_202120000_151626469_DE_00.xls.csv");
    std::vector<std::vector<std::string>> csvData = readCSV(csvPath);
    int rows = csvData.size();
    tc_login(argc, argv);

    // Vytvoøení položky
    tag_t Item = NULLTAG;
    tag_t Rev = NULLTAG;

    // Je potøeba zmìnit
    // 1. položka se vytváøí mimo for loop a vytvoøí se na ní BOM view vždy
    int ReturnCode = 0;
    if (csvData[1][1] == "") {
        int ReturnCode = ITEM_create_item(csvData[1][3].c_str(), "0000", itemType.c_str(), 0, &Item, &Rev);
    }
    else {
        int ReturnCode = ITEM_create_item(csvData[1][3].c_str(), csvData[1][1].c_str(), itemType.c_str(), 0, &Item, &Rev);
    }
    if (ReturnCode == ITK_ok)
    {
        ITEM_set_description(Item, csvData[1][4].c_str());
        ITEM_set_rev_description(Rev, csvData[1][4].c_str());

        ITEM_save_item(Item);
        ITEM_save_rev(Rev);

        change_ownership(Item, Rev, user, userGroup);

        char* Id = new char[ITEM_id_size_c + 1];
        char* Name = new char[ITEM_name_size_c + 1];

        FL_user_update_newstuff_folder(Item);
        ITEM_ask_id2(Item, &Id);
        ITEM_ask_name2(Item, &Name);

        printf("\nVytvoreni polozky: %s-%s\n", Id, Name);
        printf("Vytvoøení BOM View na itemu %s-%s\n", Id, Name);
    }
    else
        fprintf(stderr, "ERROR: Vytvoreni položky selhalo\n");

    // Vytvoøení BomView
    tag_t BomView = NULLTAG;
    AOM_lock(Item);
    PS_create_bom_view(BomView, NULL, NULL, Item, &BomView);
    AOM_save_without_extensions(BomView);
    ITEM_save_item(Item);

    // BomView Type
    tag_t BomViewType = NULLTAG;
    PS_ask_default_view_type(&BomViewType);

    // Vytvoøení BomView Revision
    tag_t BomViewRev = NULLTAG;
    AOM_lock(Rev);
    PS_create_bvr(BomView, NULL, NULL, true, Rev, &BomViewRev);
    AOM_save_without_extensions(BomViewRev);
    AOM_save_without_extensions(Rev);

    change_ownership(BomView, BomViewRev, user, userGroup);

    // for loop pro každou položku v csv file, pøedpokládá se že všechny následující položky mají ID, Name a Description ve sloupcích [3],[1] a [4]
    for (int i = 2; i < rows; i++) {
        tag_t Rev = create_item(csvData[i][3], csvData[i][1], itemType.c_str(), csvData[i][4], user, userGroup);

        int origLevel = (csvData[i][0].at(csvData[i][0].length() - 1)) - 48;
        int goalLevel = origLevel - 1;
        if (goalLevel < 1) {
            goalLevel = 1;
        }
        int currentItem = i;
        // while loop který se opakuje dokuï nenalezne položku, která má o 1 nižší level než ona samotná
        // následnì na položce vytvoøí BOM view (pokuï již neexstiuje)
        while (true) {
            int level = (csvData[currentItem][0].at(csvData[currentItem][0].length() - 1)) - 48;
            if (level == goalLevel) {
                if (origLevel == 1) {
                    tag_t* Occurrences;
                    AOM_lock(BomViewRev);
                    PS_create_occurrences(BomViewRev, Rev, NULLTAG, std::stoi(csvData[i][14]), &Occurrences);
                    AOM_save_without_extensions(BomViewRev);
                    MEM_free(Occurrences);
                }
                else {

                    tag_t* Parent = find_item(csvData[currentItem][3]);
                    int RevCount;
                    tag_t* ParentRev;
                    ITEM_find_revisions(Parent[0], "*", &RevCount, &ParentRev);

                    // kontrola zda Bom View již existuje
                    int bvr_count;
                    tag_t* bvrs;
                    ITEM_rev_list_all_bom_view_revs(ParentRev[0], &bvr_count, &bvrs);

                    if (bvr_count == 0) {
                        // Vytvoøení BomView
                        tag_t BomView = NULLTAG;
                        AOM_lock(Parent[0]);
                        PS_create_bom_view(BomView, NULL, NULL, Parent[0], &BomView);
                        //AOM_save(BomView);
                        AOM_save_without_extensions(BomView);
                        ITEM_save_item(Parent[0]);

                        // BomView Type
                        tag_t BomViewType = NULLTAG;
                        PS_ask_default_view_type(&BomViewType);

                        // Vytvoøení BomView Revision
                        tag_t SubBomViewRev = NULLTAG;
                        AOM_lock(ParentRev[0]);
                        PS_create_bvr(BomView, NULL, NULL, true, ParentRev[0], &SubBomViewRev);
                        AOM_save_without_extensions(SubBomViewRev);
                        AOM_save_without_extensions(ParentRev[0]);

                        change_ownership(BomView,SubBomViewRev, user, userGroup);

                        char* Id = new char[ITEM_id_size_c + 1];
                        char* Name = new char[ITEM_name_size_c + 1];

                        ITEM_ask_id2(Parent[0], &Id);
                        ITEM_ask_name2(Parent[0], &Name);

                        printf("Vytvoreni BOM View na itemu %s-%s\n", Id, Name);
                    }

                    ITEM_rev_list_all_bom_view_revs(ParentRev[0], &bvr_count, &bvrs);

                    tag_t* Occurrences;
                    AOM_lock(bvrs[0]);
                    PS_create_occurrences(bvrs[0], Rev, NULLTAG, std::stoi(csvData[i][14]), &Occurrences);
                    AOM_save_without_extensions(bvrs[0]);

                    MEM_free(bvrs);
                    MEM_free(ParentRev);
                    MEM_free(Occurrences);
                }
                break;
            }
            else {
                currentItem -= 1;
            }

        }
    }
    ITK_exit_module(true);
}