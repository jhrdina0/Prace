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

int change_ownership(tag_t tag1, const std::string& user, const std::string& userGroup) {

    AOM_unlock(tag1);

    tag_t userTag;
    tag_t userGroupTag;

    SA_find_user2(user.c_str(), &userTag);
    SA_find_group(userGroup.c_str(), &userGroupTag);

    AOM_set_ownership(tag1, userTag, userGroupTag);

    AOM_lock(tag1);

    return 0;
}

std::vector<tag_t> create_item(const std::string& itemId, const std::string& itemName, const std::string& itemType, const std::string& itemDesc, const std::string& user, const std::string& userGroup) {
    tag_t Item = NULLTAG;
    tag_t Rev = NULLTAG;
    std::vector<tag_t> Tags;
    int ReturnCode = ITEM_create_item(itemId.c_str(), itemName.c_str(), itemType.c_str(), 0, &Item, &Rev);
    if (ReturnCode == ITK_ok)
        {
        ITEM_set_description(Item, itemDesc.c_str());
        ITEM_set_rev_description(Rev, itemDesc.c_str());

        ITEM_save_item(Item);
        ITEM_save_rev(Rev);

        Tags.push_back(Item);
        Tags.push_back(Rev);       

        char* Id = new char[ITEM_id_size_c + 1];
        char* Name = new char[ITEM_name_size_c + 1];

        FL_user_update_newstuff_folder(Item);
        ITEM_ask_id2(Item, &Id);
        ITEM_ask_name2(Item, &Name);

        printf("\nVytvoreni polozky: %s-%s\n", Id, Name);
    }
    else
        fprintf(stderr, "ERROR: Vytvoření položky selhalo\n");

    //returns Item and ItemRevision Tag
    return Tags;
}

tag_t* find_item(const std::string& InputAttrValues) {
    // Vyhledání položek
    const char* AttrNames[1] = { ITEM_ITEM_ID_PROP };
    const char* AttrValues[1] = { InputAttrValues.c_str() };
    int ItemsCount = 0;
    //tag_t* Items = NULLTAG;
    tag_t* Items;
    ITEM_find_items_by_key_attributes(1, AttrNames, AttrValues, &ItemsCount, &Items);

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

    //vrátí tagy Itemů
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
    //proměnná, do které se postupně ukládají tagy všech objektů, kterým je potřeba na konci změnit owner a group ID
    std::vector<tag_t> allTags;

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

    // Vytvoření položky
    tag_t Item = NULLTAG;
    tag_t Rev = NULLTAG;

    // Je potřeba změnit
    // 1. položka se vytváří mimo for loop a vytvoří se na ní BOM view vždy
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

        allTags.push_back(Item);
        allTags.push_back(Rev);

        char* Id = new char[ITEM_id_size_c + 1];
        char* Name = new char[ITEM_name_size_c + 1];

        FL_user_update_newstuff_folder(Item);
        ITEM_ask_id2(Item, &Id);
        ITEM_ask_name2(Item, &Name);

        printf("\nVytvoreni polozky: %s-%s\n", Id, Name);
        printf("Vytvoření BOM View na itemu %s-%s\n", Id, Name);
    }
    else
        fprintf(stderr, "ERROR: Vytvoreni položky selhalo\n");

    // Vytvoření BomView
    tag_t BomView = NULLTAG;
    AOM_lock(Item);
    PS_create_bom_view(BomView, NULL, NULL, Item, &BomView);
    AOM_save_without_extensions(BomView);
    ITEM_save_item(Item);
    
    // BomView Type
    tag_t BomViewType = NULLTAG;
    PS_ask_default_view_type(&BomViewType);

    // Vytvoření BomView Revision
    tag_t BomViewRev = NULLTAG;
    AOM_lock(Rev);
    PS_create_bvr(BomView, NULL, NULL, true, Rev, &BomViewRev);
    AOM_save_without_extensions(BomViewRev);
    AOM_save_without_extensions(Rev);

    allTags.push_back(BomView);
    allTags.push_back(BomViewRev);

    // for loop pro každou položku v csv file, předpokládá se že všechny následující položky mají ID, Name a Description ve sloupcích [3],[1] a [4]
    for (int i = 2; i < rows; i++) {
        std::vector<tag_t> Tags = create_item(csvData[i][3], csvData[i][1], itemType.c_str(), csvData[i][4], user, userGroup);
        tag_t Rev = Tags[1];

        allTags.push_back(Tags[0]);
        allTags.push_back(Tags[1]);

        // předpokládá se že level je uveden ve sloupci [0] a je ve formátu ..2, ...3,...; nebo 1,2,3,...
        // kod by nefungoval v případě že by byl level vyšší než 9
        int origLevel = (csvData[i][0].at(csvData[i][0].length() - 1)) - 48;
        int goalLevel = origLevel - 1;
        if (goalLevel < 1) {
            goalLevel = 1;
        }
        int currentItem = i;
        // while loop který se opakuje dokuď nenalezne položku, která má o 1 nižší level než ona samotná
        // následně na položce vytvoří BOM view (pokuď již neexistuje)
        while (true) {
            int level = (csvData[currentItem][0].at(csvData[currentItem][0].length() - 1)) - 48;
            if (level == goalLevel) {
                if (origLevel == 1) {
                    tag_t* Occurrences;
                    AOM_lock(BomViewRev);
                    // předpokládá se, že počet položek je ve sloupci [14]
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
                        // Vytvoření BomView
                        tag_t BomView = NULLTAG;
                        AOM_lock(Parent[0]);
                        PS_create_bom_view(BomView, NULL, NULL, Parent[0], &BomView);
                        //AOM_save(BomView);
                        AOM_save_without_extensions(BomView);
                        ITEM_save_item(Parent[0]);

                        // BomView Type
                        tag_t BomViewType = NULLTAG;
                        PS_ask_default_view_type(&BomViewType);

                        // Vytvoření BomView Revision
                        tag_t SubBomViewRev = NULLTAG;
                        AOM_lock(ParentRev[0]);
                        PS_create_bvr(BomView, NULL, NULL, true, ParentRev[0], &SubBomViewRev);
                        AOM_save_without_extensions(SubBomViewRev);
                        AOM_save_without_extensions(ParentRev[0]);

                        allTags.push_back(BomView);
                        allTags.push_back(SubBomViewRev);

                        char* Id = new char[ITEM_id_size_c + 1];
                        char* Name = new char[ITEM_name_size_c + 1];

                        ITEM_ask_id2(Parent[0], &Id);
                        ITEM_ask_name2(Parent[0], &Name);

                        printf("Vytvoreni BOM View na itemu %s-%s\n", Id, Name);
                    }

                    ITEM_rev_list_all_bom_view_revs(ParentRev[0], &bvr_count, &bvrs);

                    tag_t* Occurrences;
                    AOM_lock(bvrs[0]);
                    // předpokládá se, že počet položek je ve sloupci [14]
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

    // Změň ownership všech Itemů, revizí a BOM views
    std::cout << "Menim ownership, utilitu zatim nevypínejte...\n";
    for (int i = 0; i < allTags.size(); i++) {
        change_ownership(allTags[i], user.c_str(), userGroup.c_str());
    }
    std::cout << "\nHotovo\n";
    ITK_exit_module(true);
}
