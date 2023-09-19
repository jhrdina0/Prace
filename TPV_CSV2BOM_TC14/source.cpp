#define  _CRT_SECURE_NO_WARNINGS 1
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
#include <iomanip>
#include <ctime>
#include <qry/qry.h>
#include <ps/ps.h>
#include <string>


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
            //druhItemu = "Item";
            druhItemu = "TPV4_Vyrobek";
        }
        else if (itemType == "M") {
            //druhItemu = "Item";
            druhItemu = "TPV4_nak_dil";
        }
        else if (itemType == "V") {
            //druhItemu = "Item";
            druhItemu = "TPV4_Vyrobek";
        }
        else {
            druhItemu = "Item";
        }
        ECHO(("Vytvoření itemu %s\n", druhItemu.c_str()));

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
        //if (ReturnCode != ITK_ok) {
        //    ECHO(("Vytvoření položky selhalo ERROR CODE: %d\n", ReturnCode));
        //}

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
    else {
        ECHO(("Item %s již existuje\n", druhItemu.c_str()));
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

tag_t create_relation(char relation_type[GRM_relationtype_name_size_c + 1], tag_t primary_object_tag, tag_t secondary_object_tag)
{
    ECHO(("L:%d partrev %d design %d \n", __LINE__, primary_object_tag, secondary_object_tag));
    tag_t relation_type_tag = NULLTAG;
    GRM_find_relation_type(relation_type, &relation_type_tag);
    ECHO(("realtion type_%d \n -primary_object_tag %d \n -secondary_object_tag %d \n", relation_type_tag, primary_object_tag, secondary_object_tag));
    tag_t relation_tag = NULLTAG;
    GRM_create_relation(primary_object_tag, secondary_object_tag, relation_type_tag, NULLTAG, &relation_tag);

    GRM_save_relation(relation_tag);
    return relation_tag;
}

static void create_dataset(char* type_name, char* name, tag_t item, tag_t rev, tag_t* dataset)
{
    char
        format_name[AE_io_format_size_c + 1] = "BINARY_REF";
    tag_t
        datasettype,
        tool;

    AE_find_datasettype2(type_name, &datasettype);
    if (datasettype == NULLTAG)
    {
        //ECHO(("Dataset Type %s not found!\n", type_name);
        exit(EXIT_FAILURE);
    }

    AE_ask_datasettype_def_tool(datasettype, &tool);

    //ECHO(("Creating Dataset: %s\n", name);
    AE_create_dataset_with_id(datasettype, name, "", "", "", dataset);
    //verze TC11  AE_create_dataset(datasettype, name, "", dataset);

    AE_set_dataset_tool(*dataset, tool);
    if (strcmp(type_name, "PDF")) strcpy(format_name, "PDF");

    AE_set_dataset_format2(*dataset, format_name);
    //ECHO(("Saving Dataset: %s\n", name);
    AOM_save_with_extensions(*dataset);

    /*attach dataset to item revision */
    create_relation((char*)"IMAN_specification", rev, *dataset);
    // ITEM_attach_rev_object(rev, *dataset, ITEM_specification_atth);
   //  ITEM_save_item(item);

}

void importDataset(tag_t dataset, char* way, char* ref, char* fileName)
{
    /*  AE_find_dataset finds latest revision of dataset */

    //IFERR_ABORT(AE_find_dataset("6667776-A", &dataset));
    //ECHO("\n dataset: %u \n", dataset);
    AOM_lock(dataset);
    AOM_refresh(dataset, TRUE);
    //  ECHO(("\n dataset=%d) \n ref=%s) \n way=%s) \n filename=%s) \n",dataset, ref, way, fileName);
      /* the fourth argument must be a unique name in the volume */
    AE_import_named_ref(dataset, ref, way, fileName, SS_BINARY);
    // AE_import_named_ref(dataset, "UG-QuickAccess-Binary", "W:\\images_preview.qaf", "6667776-A_binary.qaf",  SS_BINARY);

    AOM_save_with_extensions(dataset);
    AOM_refresh(dataset, FALSE);
    AOM_unlock(dataset);
    AOM_unload(dataset);
}

std::string getPDFname(std::string str) {
    std::replace(str.begin(), str.end(), '\\', ' ');  // replace ':' by ' '

    std::vector<std::string> array;
    std::stringstream ss(str);
    std::string temp;
    while (ss >> temp)
        array.push_back(temp);
    ECHO(("First string: %s\n", array[0]));
    int length = array.size();
    ECHO(("pdf NAME: %s\n", array[length - 1]));
    return array[length - 1];

}

int TPV_CSV2BOM_TC14(EPM_action_message_t msg)
{
    int n_attachments,
        repeats,
        num_relations;
    char
        * class_name;
    tag_t
        RootTask,
        * attachments,
        class_tag,
        window,
        tBOMTopLine,
        projRevTag,
        relation_type,
        * relation_tags,
        relation,
        vyrobky_tag;

    time_t rawtime;
    struct tm* timeinfo;
    char buffer[80];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, sizeof(buffer), "%d-%m-%Y-%H-%M-%S", timeinfo);
    std::string strTime(buffer);

    std::string csvPath = "C:\\ProgramData\\TPV_CSV2BOM_TCtemp"+strTime+".csv";
    bool exportSucces = false,
        addRelation = false;

    EPM_ask_root_task(msg.task, &RootTask);
    EPM_ask_attachments(RootTask, EPM_target_attachment, &n_attachments, &attachments);
    ECHO(("%d %d \n", __LINE__, n_attachments));

    for (int i = 0; i < n_attachments; i++)
    {
        POM_class_of_instance(attachments[i], &class_tag);
        // Returns the name of the given class
        POM_name_of_class(class_tag, &class_name);

        if (strcmp(class_name, "Dataset") == 0) {

            ECHO(("try Export Dataset\n"));
            AE_export_named_ref(attachments[i], "Text", csvPath.c_str());
            ECHO(("Export Dataset hotovo\n"));
            exportSucces = true;
        }
        if (strcmp(class_name, "TPV4_ProjektRevision") == 0) {
            ECHO(("TPV4_ProjektRevision nalezen v targetech.\n"));
            projRevTag = attachments[i];         
            addRelation = true;
            //AOM_ask_relations(attachments[i], "TPV4_Vyrobek", &num_relations, &relation_tags);
            //GRM_ask_relation_type(relation_tags[0], &relation_type);
            
        }
        ECHO(("class_name: %s \n", class_name));
    }

    if (not exportSucces) {
        ECHO(("Failed to export dataset. \n"));
        return 0;
    }
    ECHO(("Dataset exported succesfully. \n"));

    std::string user;
    std::string userGroup;
    std::string PDFname;

    // Vytvoření položky
    tag_t OrigItem = NULLTAG;
    tag_t OrigRev = NULLTAG;

    vectorArray csvData = readCSV(csvPath);
    int rows = csvData.size();

    EMH_store_error_s2(EMH_severity_warning, 1,
        "EMH_severity_warning", "ITK_ok");

    try {
        if (csvData[1].size() > 7) {
            throw 505;
        }
        if (csvData[1].size() < 5) {
            throw 506;
        }
        if (csvData[1][0] == "") {
            throw 507;
        }
    }
    catch (...) {
        ECHO(("ERROR, Špatný formát CSV!\n"));
        //EMH_store_error_s2(EMH_severity_warning, 1,"EMH_severity_warning", "ITK_ok");
        EMH_clear_errors();
        EMH_store_error_s1(EMH_severity_warning, 919002,"");
        return 0;
    }

    // 1. položka se vytváří mimo for loop a vytvoří se na ní BOM view, pokud již neexistuje
        // vytvoř vrcholový item
    std::vector<tag_t> Tags = create_item(csvData[1][2], csvData[1][3], csvData[1][1], csvData[1][5]);
    OrigItem = Tags[0];
    OrigRev = Tags[1];
    tag_t newFileTag;
    IMF_file_t fileDescriptor;

    if (csvData[1].size() > 6) {
        if (csvData[1][6] != "-") {
            if (csvData[1][6] != "") {
                tag_t dataset;
                char* pathname;

                PDFname = getPDFname(csvData[1][6]);

                create_dataset((char*)"PDF", (char*)PDFname.c_str(), OrigItem, OrigRev, &dataset);

                importDataset(dataset, (char*)csvData[1][6].c_str(), (char*)"PDF_Reference", (char*)PDFname.c_str());
            }
        }
    }

    // kontrola zda Bom View již existuje
    int bvr_count;
    tag_t* bvrs;
    ITEM_rev_list_all_bom_view_revs(OrigRev, &bvr_count, &bvrs);

    if (bvr_count == 0) {
        // Vytvoření vrcholové BomView
        tag_t BomView = NULLTAG;
        AOM_lock(OrigItem);
        PS_create_bom_view(BomView, NULL, NULL, OrigItem, &BomView);
        AOM_save_without_extensions(BomView);
        ITEM_save_item(OrigItem);

        // BomView Type
        tag_t BomViewType = NULLTAG;
        PS_ask_default_view_type(&BomViewType);

        // Vytvoření BomView Revision
        tag_t BomViewRev = NULLTAG;
        AOM_lock(OrigRev);
        PS_create_bvr(BomView, NULL, NULL, true, OrigRev, &BomViewRev);
        AOM_save_without_extensions(BomViewRev);
        AOM_save_without_extensions(OrigRev);
    }
    else {
        ECHO(("Kusovník na vrcholovém itemu již existuje.\n"));
        remove(csvPath.c_str());
        return 0;
    }
    if (addRelation) {
        //GRM_find_relation()
        ECHO(("Přidej relace.\n"));
        GRM_find_relation_type("TPV4_Vyrobky", &relation_type);
        ECHO(("Relation type: %d\n", relation_type));
        GRM_create_relation(projRevTag, OrigRev, relation_type, NULLTAG, &relation);
        ECHO(("Relace vytvořena.\n"));
        GRM_save_relation(relation);
    }

    ITEM_rev_list_all_bom_view_revs(OrigRev, &bvr_count, &bvrs);
    tag_t BomViewRev = bvrs[0];
    std::vector<int> levels(10, 0);

    // for loop pro každou položku v csv file, předpokládá se že všechny následující položky mají ID, Name, type a UOM ve sloupcích [2],[3],[1] a [5]
    for (int i = 2; i < rows; i++) {
        std::vector<tag_t> Tags = create_item(csvData[i][2], csvData[i][3], csvData[i][1], csvData[i][5]);
        tag_t Item = Tags[0];
        tag_t Rev = Tags[1];
        repeats = 0;

        if (csvData[i].size() > 6) {
            if (csvData[i][6] != "-") {
                tag_t dataset;
                char* pathname;

                PDFname = getPDFname(csvData[i][6]);

                create_dataset((char*)"PDF", (char*)PDFname.c_str(), Item, Rev, &dataset);

                importDataset(dataset, (char*)csvData[i][6].c_str(), (char*)"PDF_Reference", (char*)PDFname.c_str());
            }
        }
        else {
            ECHO(("No PDF attached. \n"));
        }

        // předpokládá se že level je uveden ve sloupci [0] a je ve formátu 1.,2..,3...,4....,
        // kod by nefungoval v případě že by byl level vyšší než 9
        int origLevel = (csvData[i][0][0]) - 48;
        int goalLevel = origLevel - 1;
        if (goalLevel < 1) {
            goalLevel = 1;
        }
        int currentItem = i;
        int n_children;
        tag_t* children;
        // while loop který se opakuje dokuď nenalezne položku, která má o 1 nižší level než ona samotná
        // následně na položce vytvoří BOM view (pokuď již neexistuje)
        while (true) {
            int level = (csvData[currentItem][0][0]) - 48;
            if (level == goalLevel) {
                if (origLevel == 1) {
                    tag_t* Occurrences;
                    AOM_lock(BomViewRev);
                    // předpokládá se, že počet položek je ve sloupci [4]

                    PS_create_occurrences(BomViewRev, Rev, NULLTAG, 1, &Occurrences);
                    
                    levels[origLevel] += 10;
                    BOM_create_window(&window);
                    BOM_set_window_top_line(window, NULLTAG, OrigRev, NULLTAG, &tBOMTopLine);
                    BOM_line_ask_child_lines(tBOMTopLine, &n_children, &children);

                    ECHO(("bl_sequence_no: %d, repeats: %d n_children: %d last children: %d\n", levels[origLevel], repeats, n_children, children[0]));
                    
                    AOM_UIF_set_value(children[0], "bl_quantity", csvData[i][4].c_str());
                    AOM_UIF_set_value(children[0], "bl_sequence_no", std::to_string(levels[origLevel]).c_str());
                    
                    AOM_save_without_extensions(BomViewRev);
                    MEM_free(Occurrences);
                    MEM_free(children);
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

                    PS_create_occurrences(bvrs[0], Rev, NULLTAG, 1, &Occurrences);
                    if (repeats == 1) {
                        levels[origLevel] = 0;
                    }

                    levels[origLevel] += 10;
                    BOM_create_window(&window);
                    BOM_set_window_top_line(window, NULLTAG, ParentRev[0], NULLTAG, &tBOMTopLine);
                    BOM_line_ask_child_lines(tBOMTopLine, &n_children, &children);
                    ECHO(("bl_sequence_no: %d, repeats: %d n_children: %d last children: %d\n", levels[origLevel], repeats, n_children, children[0]));

                    AOM_UIF_set_value(children[0], "bl_quantity", csvData[i][4].c_str());
                    AOM_UIF_set_value(children[0], "bl_sequence_no", std::to_string(levels[origLevel]).c_str());

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
                repeats += 1;
            }
        }
    }

    remove(csvPath.c_str());
    return 0;
}
