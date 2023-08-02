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
#include <algorithm>
#include <random>
#include <ps/ps.h>
#include <bom/bom.h>
#include <cstdlib>
#include <cstring>


#define TC_HANDLERS_DEBUG "TC_HANDLERS_DEBUG" 

#define ERROR_CHECK(X) (report_error( __FILE__, __LINE__, #X, (X)));
#define IFERR_REPORT(X) (report_error( __FILE__, __LINE__, #X, (X)));
#define IFERR_RETURN(X) if (IFERR_REPORT(X)) return
#define IFERR_RETURN_IT(X) if (IFERR_REPORT(X)) return X
#define ECHO(X)  printf X; TC_write_syslog X

using vectorArray = std::vector<std::vector<std::string>>;
using variablesArray = std::vector<std::pair<tag_t, std::pair<double, double>>>;

extern "C" DLLAPI int TPV_seradBOM_TC12_register_callbacks();
extern "C" DLLAPI int TPV_seradBOM_TC12_init_module(int* decision, va_list args);
int TPV_seradBOM_TC12(EPM_action_message_t msg);

extern "C" DLLAPI int TPV_seradBOM_TC12_register_callbacks()
{
    printf("Registruji handler-TPV_seradBOM_TC12_register_callbacks.dll\n");
    CUSTOM_register_exit("TPV_seradBOM_TC12", "USER_init_module", TPV_seradBOM_TC12_init_module);

    return ITK_ok;
}

extern "C" DLLAPI int TPV_seradBOM_TC12_init_module(int* decision, va_list args)
{
    *decision = ALL_CUSTOMIZATIONS;  // Execute all customizations

    // Registrace action handleru
    int Status = EPM_register_action_handler("TPV_seradBOM_TC12", "", TPV_seradBOM_TC12);
    if (Status == ITK_ok) printf("Handler pro serazeni Kusovniku %s \n", "TPV_seradBOM_TC12");


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
    std::string dateString = std::to_string(date.day) + " " +
        std::to_string(date.month + 1) + "-" +
        std::to_string(date.year) + "-" +
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

double convertToDouble(const char* str) {
    char* endPtr;
    double result = std::strtod(str, &endPtr);

    // If the conversion fails, endPtr will point to the same location as str
    // This indicates an invalid conversion (e.g., an empty string, non-numeric characters)
    if (endPtr == str) {
        // Handle the error or return an appropriate value (e.g., 0.0)
        // For simplicity, I'm returning 0.0 here
        return 0.0;
    }

    return result;
}

bool hasEnding(const char* fullString, std::string const& ending) {
    size_t fullLength = strlen(fullString);
    size_t endingLength = ending.length();

    if (fullLength >= endingLength) {
        return (0 == strcmp(fullString + fullLength - endingLength, ending.c_str()));
    }
    else {
        return false;
    }
}

char* removeLastN(char* str, int n) {
    size_t length = strlen(str);
    if (length < static_cast<size_t>(n)) {
        return str;
    }

    strcpy_s(str + length - n, length + 1 - (length - n), str + length);
    return str;
}

char* replaceCommaWithPeriod(char* str) {
    if (str == nullptr) {
        return str; // Handle null pointer input
    }

    size_t length = strlen(str);
    for (size_t i = 0; i < length; i++) {
        if (str[i] == ',') {
            str[i] = '.'; // Replace ',' with '.'
        }
    }

    return str;
}

int RecursiveBOM(const std::string& InputAttrValues, const std::string& RevId)
{
    int n_children;
    tag_t
        window,
        tBOMTopLine,
        * children;

    std::vector<tag_t> ItemAndRev = find_itemRevision(InputAttrValues, RevId);
    tag_t Item = ItemAndRev[0];
    tag_t ItemRev = ItemAndRev[1];

    ECHO(("ItemTag: %d Current Revision Tag: %d\n", Item, ItemRev));

    BOM_create_window(&window);
    BOM_set_window_top_line(window, NULLTAG, ItemRev, NULLTAG, &tBOMTopLine);
    BOM_line_ask_child_lines(tBOMTopLine, &n_children, &children);

    ECHO(("Poèet children: %d\n", n_children));
    variablesArray ListHodnot;
    // Iterate over chlidren (if there are any) and call recursiveBOM function on them
    for (int i = 0; i < n_children; i++)
    {
        const std::string ParentId = InputAttrValues;
        char
            * ItemId,
            * RevId,
            * kodPuvoduStr,
            * hmotnostStr;
        double
            kodPuvodu,
            hmotnost;

        std::pair<tag_t, std::pair<double, double>> row;

        AOM_ask_value_string(children[i], "bl_item_item_id", &ItemId);
        AOM_ask_value_string(children[i], "bl_rev_item_revision_id", &RevId);

        ItemAndRev = find_itemRevision(ItemId, RevId);
        Item = ItemAndRev[0];
        ItemRev = ItemAndRev[1];

        int n_ifails;
        const int* severities;
        const int* ifails;
        const char** texts;


        // Test Jestli výpis errorù funguje správnì

        //char* testError;
        //int return_code = AOM_ask_value_string(ItemRev, "testError", &testError);
        //if (return_code != ITK_ok) {
        //    EMH_ask_errors(&n_ifails, &severities, &ifails, &texts);
        //    ECHO(("n_ifails: %d", n_ifails));
        //    for (int i = 0; i < n_ifails; i++) {
        //        ECHO(("ERROR: %s\n", texts[i]));
        //    }
        //}

        ECHO(("Item TAG: %d ItemRevision TAG: %d\n", Item, ItemRev));

        int return_code = AOM_ask_value_string(children[i], "bl_MA4DesignPartRevision_ma4PartType", &kodPuvoduStr);
        if (return_code != ITK_ok) {
            EMH_ask_errors(&n_ifails, &severities, &ifails, &texts);
            for (int i = 0; i < n_ifails; i++) {
                ECHO(("n_ifails: %d\n", n_ifails));
                ECHO(("ERROR: %s\n", texts[i]));
            }
        }

        double hmotnostDouble;
        return_code = AOM_ask_value_double(ItemRev, "ma4Weight", &hmotnostDouble);
        if (return_code != ITK_ok) {
            EMH_ask_errors(&n_ifails, &severities, &ifails, &texts);
            for (int i = 0; i < n_ifails; i++) {
                ECHO(("n_ifails: %d\n", n_ifails));
                ECHO(("ERROR: %s\n", texts[i]));
            }
        }

        if (hmotnostDouble == 0) {
            ECHO(("ma4Weight nenalezeno, zeptat se na ma4Weight_S z bomline\n"));

            AOM_ask_value_string(children[i], "bl_MA4DesignPartRevision_ma4Weight_S", &hmotnostStr);

            if (hmotnostStr == nullptr || strlen(hmotnostStr) == 0)
            {
                // If empty, allocate memory for the "0" string and copy it to hmotnost
                char* newString = new char[2]; // "0" + null terminator
                strcpy_s(newString, 2, "0");
                hmotnostStr = newString;
            }

            if (hasEnding(hmotnostStr, " kg")) {
                hmotnostStr = removeLastN(hmotnostStr, 3);
            }

            int length = strlen(hmotnostStr);
            char* hmotnostFinal;

            // Check if the length is greater than 13
            if (length > 13) {
                char truncHmotnost[14]; // Nový string
                strncpy_s(truncHmotnost, hmotnostStr, 13); // Zkopíruj první 13 charakterù
                truncHmotnost[13] = '\0';
                hmotnostFinal = truncHmotnost;
            }
            else {
                hmotnostFinal = hmotnostStr;
            }

            std::string a, b, c, d;

            a = ItemId;
            b = kodPuvoduStr;
            c = hmotnostFinal;
            d = RevId;

            ECHO(("ItemID: %s, RevId: %s, ma4PartType: %s, ma4Weight_s %s\n", a, d, b, c));

            hmotnostFinal = replaceCommaWithPeriod(hmotnostFinal);

            kodPuvodu = convertToDouble(kodPuvoduStr);
            hmotnost = convertToDouble(hmotnostFinal);

            AOM_lock(ItemRev);
            AOM_set_value_double(ItemRev, "ma4Weight", hmotnost);
            AOM_save_without_extensions(ItemRev);
            //MEM_free(hmotnostStr);
        }
        else
        {
            hmotnost = hmotnostDouble;

            std::string a, b, c, d;

            a = ItemId;
            b = kodPuvoduStr;
            d = RevId;

            ECHO(("ItemID: %s, RevId: %s, ma4PartType: %s, ma4Weight %f\n", a, d, b, hmotnost));
        }

        kodPuvodu = convertToDouble(kodPuvoduStr);

        row.first = children[i];
        row.second.first = kodPuvodu;
        row.second.second = hmotnost;

        ListHodnot.push_back(row);

        // push new row into the csv

        RecursiveBOM(ItemId, RevId);

        MEM_free(kodPuvoduStr);
        MEM_free(ItemId);
        MEM_free(RevId);
    }
    if (n_children > 0) {

        std::sort(ListHodnot.begin(), ListHodnot.end(), [](const auto& a, const auto& b) {
            if (a.second.first != b.second.first)
                return a.second.first < b.second.first;
            return a.second.second > b.second.second; // > descending, < ascending
            });

        ECHO(("\n***********ITEM CHILDREN SEØAZENO***************\n"));

        int bl_sequence_no = 0;
        for (const auto& item : ListHodnot) {
            bl_sequence_no += 1;
            ECHO(("%d Tag: %d, KodPuvodu: %f, Hmotnost: %f\n", bl_sequence_no, item.first, item.second.first, item.second.second));
            int AttributeId;
            BOM_line_look_up_attribute("bl_sequence_no", &AttributeId);
            BOM_line_set_attribute_string(item.first, AttributeId, std::to_string(bl_sequence_no).c_str());
        }
        ECHO(("\n"));


        
        //for (int i = 0; i = ListHodnot.size(); i++) {
        //    ECHO(("Tag: %d, KodPuvodu: %f, Hmotnost: %f\n", ListHodnot[i].first, ListHodnot[i].second.first, ListHodnot[i].second.second));
        //}
    }

    MEM_free(children);
    BOM_save_window(window);
    BOM_close_window(window);

    return 0;
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
    // TEST
    return config;
}

std::string read_properties(EPM_action_message_t msg)
{
    char
        * Argument,
        * Flag,
        * Value;
    std::string
        AttachPath,
        config,
        itemType;
    std::vector<std::string> props;

    int ArgumentCount = TC_number_of_arguments(msg.arguments);

    while (ArgumentCount-- > 0)
    {
        Argument = TC_next_argument(msg.arguments);
        ITK_ask_argument_named_value(Argument, &Flag, &Value);
        if (strcmp("itemType", Flag) == 0 && Value != nullptr)
        {
            itemType = Value;
        }
        MEM_free(Flag);
        MEM_free(Value);
    }

    return itemType;
}

int TPV_seradBOM_TC12(EPM_action_message_t msg)
{
    int n_attachments;
    char
        * class_name,
        * ItemId,
        * RevId;
    tag_t
        RootTask,
        * attachments,
        class_tag;

    EPM_ask_root_task(msg.task, &RootTask);
    EPM_ask_attachments(RootTask, EPM_target_attachment, &n_attachments, &attachments);

    std::string itemType = read_properties(msg);

    ECHO(("\n**************zacatek seradBOM workflow*************\n"));

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
            std::string a = ItemId;
            std::string b = RevId;
            ECHO(("ItemID: %s Current Revision ID: %s\n", a, b));

            RecursiveBOM(ItemId, RevId);

        }
    }
    ECHO(("\n**************konec seradBOM workflow*************\n"));
    return 0;
}