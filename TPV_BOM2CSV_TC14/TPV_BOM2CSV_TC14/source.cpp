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
using vectorArray = std::vector<std::vector<std::string>>;

extern "C" DLLAPI int TPV_BOM2CSV_TC14_register_callbacks();
extern "C" DLLAPI int TPV_BOM2CSV_TC14_init_module(int* decision, va_list args);
int TPV_BOM2CSV_TC14(EPM_action_message_t msg);

extern "C" DLLAPI int TPV_BOM2CSV_TC14_register_callbacks()
{
    printf("Registruji handler-TPV_BOM2CSV_TC14_register_callbacks.dll\n");
    CUSTOM_register_exit("TPV_BOM2CSV_TC14", "USER_init_module", TPV_BOM2CSV_TC14_init_module);

    return ITK_ok;
}

extern "C" DLLAPI int TPV_BOM2CSV_TC14_init_module(int* decision, va_list args)
{
    *decision = ALL_CUSTOMIZATIONS;  // Execute all customizations

    // Registrace action handleru
    int Status = EPM_register_action_handler("TPV_BOM2CSV", "", TPV_BOM2CSV_TC14);
    if (Status == ITK_ok) printf("Handler pro exportovani dat z BOM do CSV %s \n", "TPV_BOM2CSV_TC14");


    return ITK_ok;
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

int get_string(tag_t object, const char* prop_name, char** value)
{
    char* orig_string;
    int return_int = AOM_ask_value_string(object, prop_name, &orig_string);
    *value = orig_string;
    return return_int;
}

static tag_t ask_item_revision_from_bom_line(tag_t bom_line)
{
    tag_t
        item_revision = NULLTAG;
    char
        * item_id = NULL,
        * rev_id = NULL;

    get_string(bom_line, "bl_item_item_id", &item_id);
    get_string(bom_line, "bl_rev_item_revision_id", &rev_id);
    ITEM_find_rev(item_id, rev_id, &item_revision);

    if (item_id) MEM_free(item_id);
    if (rev_id) MEM_free(rev_id);
    return item_revision;
}

std::string convertDateToString(const date_t& date) {
    std::string dateString = std::to_string(date.year) + "-" +
        std::to_string(date.month) + "-" +
        std::to_string(date.day) + " " +
        std::to_string(date.hour) + ":" +
        std::to_string(date.minute) + ":" +
        std::to_string(date.second);
    return dateString;
}

std::string readElement(tag_t Tag, std::string property_name) {
    PROP_value_type_t valtype;
    char* valtype_n;
    AOM_ask_value_type(Tag, property_name.c_str(), &valtype, &valtype_n);
    MEM_free(valtype_n);
    if (valtype == PROP_char) {
        char value;
        AOM_ask_value_char(Tag, property_name.c_str(), &value);
        return std::string(1, value);
    }
    else if (valtype == PROP_date) {
        date_t value;
        AOM_ask_value_date(Tag, property_name.c_str(), &value);
        return convertDateToString(value);
    }
    else if (valtype == PROP_double) {
        double value;
        AOM_ask_value_double(Tag, property_name.c_str(), &value);
        return std::to_string(value);
    }
    else if (valtype == PROP_float) {
        // Handle float type
    }
    else if (valtype == PROP_int) {
        int value;
        AOM_ask_value_int(Tag, property_name.c_str(), &value);
        return std::to_string(value);
    }
    else if (valtype == PROP_logical) {
        bool value;
        AOM_ask_value_logical(Tag, property_name.c_str(), &value);
        return value ? "true" : "false";
    }
    else if (valtype == PROP_short) {
        // Handle short type
    }
    else if (valtype == PROP_string) {
        char* value;
        AOM_ask_value_string(Tag, property_name.c_str(), &value);
        return value;
    }
    else if (valtype == PROP_typed_reference) {
        tag_t value_tag;
        char* value;
        AOM_ask_value_tag(Tag, property_name.c_str(), &value_tag);
        if (value_tag == 0) {
            return "ks";
        }
        else {
            AOM_ask_value_string(value_tag, "object_string", &value);
            return value;
        }
    }
    else if (valtype == PROP_untyped_reference) {
        tag_t value_tag;
        char* value;
        AOM_ask_value_tag(Tag, property_name.c_str(), &value_tag);
        if (value_tag == 0) {
            return "";
        }
        else {
            AOM_ask_value_string(value_tag, "object_string", &value);
            return value;
        }
    }
    // If the valtype is not handled, return an empty string
    return "";
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string item;

    while (getline(ss, item, delim)) {
        result.push_back(item);
    }

    return result;
}

std::string ExportAttachments(tag_t ItemRev, std::string Values, std::string Path, std::string ItemId) {

    std::vector<std::string> splittedValues = split(Values, ',');
    std::string AttachType = splittedValues[0];
    std::string Relation = splittedValues[1];

    std::string cesta;

    tag_t relationType;

    GRM_find_relation_type(Relation.c_str(), &relationType);

    int n_specs;
    tag_t* specs;
    char* type_name;
    char* dataset_name;
    GRM_list_secondary_objects_only(ItemRev, relationType, &n_specs, &specs);

    for (int i = 0; i < n_specs; i++) {

        tag_t type_tag;
        char* type_name;
        char* dataset_name;
        TCTYPE_ask_object_type(specs[i], &type_tag);
        TCTYPE_ask_name2(type_tag, &type_name);
        AOM_ask_value_string(specs[i], "object_string", &dataset_name);

        std::string s = type_name;
        if (s == "PDF" and AttachType == "PDF") {
            std::string s = dataset_name;
            cesta = Path +"PDF\\"+ ItemId + s;
            AE_export_named_ref(specs[i], "PDF_Reference", cesta.c_str());
        }
        if (s == "DXF" and AttachType == "DXF") {
            std::string s = dataset_name;
            cesta = Path + "DXF\\" + ItemId + s;
            AE_export_named_ref(specs[i], "DXF", cesta.c_str());
        }
        MEM_free(dataset_name);
        MEM_free(type_name);
    }
    if (n_specs == 0){
        return "-";
    }
    else {
        return cesta;
    }

}

vectorArray RecursiveBOM(const std::string& InputAttrValues, const std::string& RevId, vectorArray csv, vectorArray config, bool addParent, std::string AttachPath)
{
    int n_children;
    tag_t
        window,
        tBOMTopLine,
        * children;

    std::vector<tag_t> ItemAndRev;
    ItemAndRev = find_itemRevision(InputAttrValues, RevId);
    tag_t Item = ItemAndRev[0];
    tag_t ItemRev = ItemAndRev[1];

    BOM_create_window(&window);
    BOM_set_window_top_line(window, NULLTAG, ItemRev, NULLTAG, &tBOMTopLine);

    BOM_line_ask_child_lines(tBOMTopLine, &n_children, &children);

    if (addParent) {
        const std::string ParentId = InputAttrValues;
        char
            * ItemId,
            * RevId;
        tag_t Tag = tBOMTopLine;
        std::vector<std::string> csv_row;

        AOM_ask_value_string(tBOMTopLine, "bl_item_item_id", &ItemId);
        AOM_ask_value_string(tBOMTopLine, "bl_rev_item_revision_id", &RevId);

        ItemAndRev = find_itemRevision(ItemId, RevId);
        Item = ItemAndRev[0];
        ItemRev = ItemAndRev[1];

        for (int j = 0; j < config.size(); j++) {
            std::string value;
            if (config[j][0] == "BOM") {
                value = readElement(tBOMTopLine, config[j][1]);
            }
            if (config[j][0] == "Item") {
                value = readElement(Item, config[j][1]);
            }
            if (config[j][0] == "Rev") {
                value = readElement(ItemRev, config[j][1]);
            }
            if (config[j][0] == "Export") {
                value = ExportAttachments(ItemRev, config[j][1], AttachPath, ItemId);
            }

            //všechny ; symboly jsou nahrazeny , aby se nepoškodil formát csv
            std::replace(value.begin(), value.end(), ';', ',');

            csv_row.push_back(value);
        }

        // push new row into the csv
        csv.push_back(csv_row);

        MEM_free(ItemId);
        MEM_free(RevId);
    }

    // Iterate over chlidren (if there are any) and call recursiveBOM function on them
    for (int i = 0; i < n_children; i++)
    {
        const std::string ParentId = InputAttrValues;
        char
            * ItemId,
            * RevId;
        tag_t Tag = children[i];
        std::vector<std::string> csv_row;


        AOM_ask_value_string(children[i], "bl_item_item_id", &ItemId);
        AOM_ask_value_string(children[i], "bl_rev_item_revision_id", &RevId);

        ItemAndRev = find_itemRevision(ItemId, RevId);
        Item = ItemAndRev[0];
        ItemRev = ItemAndRev[1];

        for (int j = 0; j < config.size(); j++) {
            std::string value;
            if (config[j][0] == "BOM") {
                value = readElement(children[i], config[j][1]);
            }
            if (config[j][0] == "Item") {
                value = readElement(Item, config[j][1]);
            }
            if (config[j][0] == "Rev") {
                value = readElement(ItemRev, config[j][1]);
            }
            if (config[j][0] == "Export") {
                value = ExportAttachments(ItemRev, config[j][1], AttachPath, ItemId);
            }

            //všechny ; symboly jsou nahrazeny , aby se nepoškodil formát csv
            std::replace(value.begin(), value.end(), ';', ',');

            csv_row.push_back(value);
        }

        // push new row into the csv
        csv.push_back(csv_row);

        csv = RecursiveBOM(ItemId, RevId, csv, config, false, AttachPath);

        MEM_free(ItemId);
        MEM_free(RevId);
    }

    MEM_free(children);
    BOM_save_window(window);
    BOM_close_window(window);

    return csv;
}

int createCSV(vectorArray csv, std::string exportFolderPath, vectorArray config, char* ItemId, char* RevId)
{
    std::ofstream file;
    std::string filePath = exportFolderPath + "\\export" + ItemId + "-" + RevId + ".csv";
    file.open(filePath);
    for (int j = 0; j < config.size(); j++)
    {
        file << config[j][1] << ";";
    }
    for (int i = 0; i < csv.size(); i++) {
        file << "\n";
        file << csv[i][0];
        for (int j = 1; j < csv[0].size(); j++) {
            file << ";";
            file << csv[i][j];
        }
    }
    file.close();
    return 0;
}

vectorArray load_config(std::string configPath) 
{
    vectorArray config;

    // Open the file
    std::ifstream file(configPath);

    if (file.is_open()) {
        std::string line;

        // Read each line of the file
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string key, value;

            if (std::getline(iss, key, ':') && std::getline(iss, value)) {
                key.erase(0, key.find_first_not_of(" \t\r\n"));
                key.erase(key.find_last_not_of(" \t\r\n") + 1);
                value.erase(0, value.find_first_not_of(" \t\r\n"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);

                config.push_back({ key, value });
            }
        }

        file.close();
    }
    else {
        std::cout << "Failed to open the file." << std::endl;
    }

    return config;
}

std::vector<std::string> read_properties(EPM_action_message_t msg)
{
    char
        * Argument,
        * Flag,
        * Value;
    std::string 
        AttachPath,
        config;
    std::vector<std::string> props;

    int ArgumentCount = TC_number_of_arguments(msg.arguments);

    while (ArgumentCount-- > 0)
    {
        Argument = TC_next_argument(msg.arguments);
        ITK_ask_argument_named_value(Argument, &Flag, &Value);
        if (strcmp("ConfigPath", Flag) == 0 && Value != nullptr)
        {
            config = Value;
        }
        if (strcmp("ExportFolderPath", Flag) == 0 && Value != nullptr)
        {
            AttachPath = Value;
        }
        MEM_free(Flag);
        MEM_free(Value);
    }
    props.push_back(config);
    props.push_back(AttachPath);

    return props;
}

int TPV_BOM2CSV_TC14(EPM_action_message_t msg)
{
    vectorArray 
        csv,
        config;
    std::string AttachPath;

    int n_attachments;
    char
        * class_name,
        * ItemId,
        * RevId;
    tag_t
        RootTask,
        * attachments,
        class_tag;

    std::vector<std::string> props = read_properties(msg);

    config = load_config(props[0]);
    AttachPath = props[1];

    std::string exportFolderPath = config[0][1];
    std::string itemType = config[1][1];
    config.erase(config.begin(), config.begin() + 2);

    EPM_ask_root_task(msg.task, &RootTask);
    EPM_ask_attachments(RootTask, EPM_target_attachment, &n_attachments, &attachments);

    for (int i = 0; i < n_attachments; i++)
    {

        POM_class_of_instance(attachments[i], &class_tag);

        // Returns the name of the given class
        POM_name_of_class(class_tag, &class_name);

        const char* c = itemType.c_str();
        if (strcmp(class_name, c) == 0)
        {
            AOM_ask_value_string(attachments[i], "item_id", &ItemId);
            AOM_ask_value_string(attachments[i], "current_revision_id", &RevId);

            csv = RecursiveBOM(ItemId, RevId, csv, config, true, AttachPath);

            createCSV(csv, exportFolderPath, config, ItemId, RevId);
        }
    }
    return 0;
}
