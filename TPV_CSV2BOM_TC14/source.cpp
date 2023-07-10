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
#include <algorithm>
#include <iostream>
#include <string.h>
#include <ps/ps.h>
#include <bom/bom.h>
#include <tc/folder.h>
#include <epm/epm.h>
#include <time.h>
#include <errno.h>
#include <ict/ict_userservice.h>
#include <tccore/custom.h>
#include <user_exits/user_exits.h>
#include <ps/ps.h>
#include <bom/bom.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <tccore/uom.h>
#include <ae\ae.h>
#include <tccore\grm.h>


#define TC_HANDLERS_DEBUG "TC_HANDLERS_DEBUG" 

extern "C" DLLAPI int TPV_CSV2BOM_TC14_register_callbacks();
extern "C" DLLAPI int TPV_CSV2BOM_TC14_init_module(int* decision, va_list args);
int TPV_CSV2BOM_TC14(EPM_action_message_t msg);

extern "C" DLLAPI int TPV_CSV2BOM_TC14_register_callbacks()
{
    printf("Registruji handler-TPV_CSV2BOM_TC14_register_callbacks.dll\n");
    CUSTOM_register_exit("TPV_CSV2BOM_TC14", "USER_init_module", TPV_CSV2BOM_TC14_init_module);

    return ITK_ok;
}

extern "C" DLLAPI int TPV_CSV2BOM_TC14_init_module(int* decision, va_list args)
{
    *decision = ALL_CUSTOMIZATIONS;  // Execute all customizations

    // Registrace action handleru
    int Status = EPM_register_action_handler("TPV_CSV2BOM_TC14", "", TPV_CSV2BOM_TC14);
    if (Status == ITK_ok) printf("Handler pro import dat z CSV do teamcentru BOM % s \n", "TPV_CSV2BOM_TC14");


    return ITK_ok;
}

/// reportovani by Gtac

#define ERROR_CHECK(X) (report_error( __FILE__, __LINE__, #X, (X)));
#define IFERR_REPORT(X) (report_error( __FILE__, __LINE__, #X, (X)));
#define IFERR_RETURN(X) if (IFERR_REPORT(X)) return
#define IFERR_RETURN_IT(X) if (IFERR_REPORT(X)) return X
#define ECHO(X)  printf X; TC_write_syslog X

using vectorArray = std::vector<std::vector<std::string>>;

bool ItemExists(const std::string& itemId) {
    const char* AttrNames[1] = { ITEM_ITEM_ID_PROP };
    const char* AttrValues[1] = { itemId.c_str() };
    int ItemsCount = 0;
    //tag_t* Items = NULLTAG;
    tag_t* Items;
    ITEM_find_items_by_key_attributes(1, AttrNames, AttrValues, &ItemsCount, &Items);

    if (ItemsCount == 0)
    {
        return false;
    }
    else {
        return true;
    }
}

std::vector<tag_t> find_itemRevision(const std::string& InputAttrValues, const std::string& RevId) {
    std::vector<tag_t> ItemAndRev;
    // Vyhledání položek
    const char* AttrNames[1] = { ITEM_ITEM_ID_PROP };
    const char* AttrValues[1] = { InputAttrValues.c_str() };
    int ItemsCount = 0;
    //tag_t* Items = NULLTAG;
    tag_t* Items;
    ITEM_find_items_by_key_attributes(1, AttrNames, AttrValues, &ItemsCount, &Items);
    ItemAndRev.push_back(Items[0]);

    tag_t Rev;
    ITEM_find_revision(Items[0], RevId.c_str(), &Rev);
    ItemAndRev.push_back(Rev);
    MEM_free(Items);
    //vrátí tag revize Itemu
    return ItemAndRev;
}

std::vector<tag_t> create_item(std::string itemId, std::string itemName, std::string itemType, std::string uom) {
    std::vector<tag_t> Tags;
    std::string druhItemu;

    if (not ItemExists(itemId)) {
        if (itemType == "D") {
            druhItemu = "TPV4_Vyrobek";
        }
        else if(itemType == "M") {
            druhItemu = "TPV4_nak_dil";
        }
        else {
            druhItemu = "Item";
        }

        ECHO(("druh Itemu: %s\n", druhItemu.c_str()));

        tag_t itemType,
            create_input,
            bo,
            uom_tag;


        TCTYPE_ask_type(druhItemu.c_str(), &itemType);
        TCTYPE_construct_create_input(itemType, &create_input);
        AOM_set_value_string(create_input, "item_id", itemId.c_str());
        AOM_set_value_string(create_input, "object_name", itemName.c_str());
        int ReturnCode = TCTYPE_create_object(create_input, &bo);

        // najde UOM tag podle symbolu
        // pokud symbol není nalezen, UOM zůstane na defaultní hodnotě (each)
        UOM_find_by_symbol(uom.c_str(), &uom_tag);
        ITEM_set_unit_of_measure(bo, uom_tag);
        if (ReturnCode != ITK_ok) {
            ECHO(("Vytvoření položky selhalo ERROR CODE: %d\n", ReturnCode));
        }

        AOM_save_without_extensions(bo);

        //std::string druhItemu;
        //int ReturnCode = ITEM_create_item(itemId.c_str(), itemName.c_str(), "TPV4_dilec", 0, &Item, &Rev);
        //ECHO(("Item vytvořen: %s\n.", itemId.c_str()));
        //if (ReturnCode == ITK_ok)
        //{
        //    ITEM_save_item(Item);
        //    ITEM_save_rev(Rev);

        //    Tags.push_back(Item);
        //    Tags.push_back(Rev);

        //}
        //else
        //    ECHO(("Failed to create item. \n"));
    }
    Tags = find_itemRevision(itemId, "*");
    //returns Item and ItemRevision Tag
    return Tags;
}

tag_t* find_item(const std::string& InputAttrValues) {
    // Vyhledání položek
    const char* AttrNames[1] = { ITEM_ITEM_ID_PROP };
    const char* AttrValues[1] = { InputAttrValues.c_str() };
    int ItemsCount = 0;
    tag_t* Items;
    ITEM_find_items_by_key_attributes(1, AttrNames, AttrValues, &ItemsCount, &Items);

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

vectorArray readCSV(const std::string& filename) {
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

std::string read_properties(EPM_action_message_t msg)
{
    char
        * Argument,
        * Flag,
        * Value,
        * atribut;

    int ArgumentCount = TC_number_of_arguments(msg.arguments);

    atribut = 0;
    while (ArgumentCount-- > 0)
    {
        Argument = TC_next_argument(msg.arguments);
        ITK_ask_argument_named_value(Argument, &Flag, &Value);
        if (strcmp("atribut", Flag) == 0 && Value != nullptr)
        {
            atribut = Value;
        }
        
        MEM_free(Flag);
        MEM_free(Value);
    }

    return atribut;
}

std::vector<std::string> splitString(const std::string& inputString)
{
    std::vector<std::string> outputStrings;
    std::istringstream iss(inputString);
    std::string token;

    while (std::getline(iss, token, ' '))
    {
        outputStrings.push_back(token);
    }

    return outputStrings;
}

std::string connectStrings(const std::vector<std::string>& strings, const std::string& startMarker, const std::string& endMarker)
{
    std::string connectedString;
    bool isInside = false;

    for (const std::string& str : strings)
    {
        if (str == startMarker)
        {
            isInside = true;
            continue;
        }

        if (isInside)
        {
            if (str == endMarker)
            {
                break;
            }

            connectedString += str;
            connectedString += " ";
        }
    }

    return connectedString;
}

int TPV_CSV2BOM_TC14(EPM_action_message_t msg)
{
    int n_attachments;
    char
        * class_name;
    tag_t
        RootTask,
        * attachments,
        class_tag;
    std::string csvPath = "C:\\ProgramData\\TPV_CSV2BOM_TCtemp.csv";
    bool exportSucces = false;

    EPM_ask_root_task(msg.task, &RootTask);
    EPM_ask_attachments(RootTask, EPM_target_attachment, &n_attachments, &attachments);
    ECHO(("%d %d \n", __LINE__, n_attachments));

    for (int i = 0; i < n_attachments; i++)
    {
        POM_class_of_instance(attachments[i], &class_tag);
        // Returns the name of the given class
        POM_name_of_class(class_tag, &class_name);

        if (strcmp(class_name, "Dataset") == 0) {

            ECHO(("try Export Dataset"));
            AE_export_named_ref(attachments[i], "Text", csvPath.c_str());
            ECHO(("Export Dataset hotovo"));
            exportSucces = true;
        }

        ECHO(("class_name: %s \n",class_name));


    }

    if (not exportSucces) {
        ECHO(("Failed to export dataset. \n"));
        return 0;
    }
    ECHO(("Dataset exported succesfully. \n"));

    std::string user;
    std::string userGroup;

    // Vytvoření položky
    tag_t Item = NULLTAG;
    tag_t Rev = NULLTAG;

    vectorArray csvData = readCSV(csvPath);
    int rows = csvData.size();
    std::vector<std::string> Karta = splitString(csvData[0][0]);
    std::string nazevKarty = connectStrings(Karta, "-", ";");

    //example output: "Karta:  202120011_07  - Auslegearm re. geschw. 10 meter;;;;;"
    /*
    Karta:

    202120011_07

    -
    Auslegearm
    re.
    geschw.
    10
    meter;;;;;
    ¨*/
    

    // Je potřeba změnit
    // 1. položka se vytváří mimo for loop a vytvoří se na ní BOM view vždy

    // vytvoř vrcholový item
    std::vector<tag_t> Tags = create_item(Karta[2], nazevKarty, "D", "ks");
    Item = Tags[0];
    Rev = Tags[1];

    // kontrola zda Bom View již existuje
    int bvr_count;
    tag_t* bvrs;
    ITEM_rev_list_all_bom_view_revs(Rev, &bvr_count, &bvrs);

    if (bvr_count == 0) {
        // Vytvoření vrcholové BomView
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
    }

    ITEM_rev_list_all_bom_view_revs(Rev, &bvr_count, &bvrs);
    tag_t BomViewRev = bvrs[0];

    // for loop pro každou položku v csv file, předpokládá se že všechny následující položky mají ID, Name a Description ve sloupcích [2],[3] a [1]
    for (int i = 3; i < rows; i++) {
        std::vector<tag_t> Tags = create_item(csvData[i][2], csvData[i][3], csvData[i][1], csvData[i][5]);
        tag_t Item = Tags[0];
        tag_t Rev = Tags[1];

        // předpokládá se že level je uveden ve sloupci [0] a je ve formátu 1.,2..,3...,4....,
        // kod by nefungoval v případě že by byl level vyšší než 9
        int origLevel = (csvData[i][0][0]) - 48;
        int goalLevel = origLevel - 1;
        if (goalLevel < 1) {
            goalLevel = 1;
        }
        int currentItem = i;
        // while loop který se opakuje dokuď nenalezne položku, která má o 1 nižší level než ona samotná
        // následně na položce vytvoří BOM view (pokuď již neexistuje)
        while (true) {
            int level = (csvData[currentItem][0][0]) - 48;
            if (level == goalLevel) {
                if (origLevel == 1) {
                    tag_t* Occurrences;
                    AOM_lock(BomViewRev);
                    // předpokládá se, že počet položek je ve sloupci [4]
                    PS_create_occurrences(BomViewRev, Rev, NULLTAG, std::stoi(csvData[i][4]), &Occurrences);
                    AOM_save_without_extensions(BomViewRev);
                    MEM_free(Occurrences);
                }
                else {
                    tag_t* Parent = find_item(csvData[currentItem][2]);
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

                    }

                    ITEM_rev_list_all_bom_view_revs(ParentRev[0], &bvr_count, &bvrs);

                    tag_t* Occurrences;
                    AOM_lock(bvrs[0]);
                    // předpokládá se, že počet položek je ve sloupci [4]
                    PS_create_occurrences(bvrs[0], Rev, NULLTAG, std::stoi(csvData[i][4]), &Occurrences);
                    AOM_save_without_extensions(bvrs[0]);

                    MEM_free(Parent);
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

    remove(csvPath.c_str());
    return 0;
}
