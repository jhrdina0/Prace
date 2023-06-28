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

extern "C" DLLAPI int TPV_odstran_kg_TC14_register_callbacks();
extern "C" DLLAPI int TPV_odstran_kg_TC14_init_module(int* decision, va_list args);
int TPV_odstran_kg_TC14(EPM_action_message_t msg);

extern "C" DLLAPI int TPV_odstran_kg_TC14_register_callbacks()
{
    printf("Registruji handler-TPV_odstran_kg_TC14_register_callbacks.dll\n");
    CUSTOM_register_exit("TPV_odstran_kg_TC14", "USER_init_module", TPV_odstran_kg_TC14_init_module);

    return ITK_ok;
}

extern "C" DLLAPI int TPV_odstran_kg_TC14_init_module(int* decision, va_list args)
{
    *decision = ALL_CUSTOMIZATIONS;  // Execute all customizations

    // Registrace action handleru
    int Status = EPM_register_action_handler("TPV_odstran_kg_TC14", "", TPV_odstran_kg_TC14);
    if (Status == ITK_ok) printf("Handler pro odstraneni , kg, z atributu tpv4_hmotnost % s \n", "TPV_odstran_kg_TC14");


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

bool hasEnding(std::string const& fullString, std::string const& ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    }
    else {
        return false;
    }
}

std::string removeLastN(std::string const& str, int n) {
    if (str.length() < n) {
        return str;
    }

    return str.substr(0, str.length() - n);
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

    BOM_create_window(&window);
    BOM_set_window_top_line(window, NULLTAG, ItemRev, NULLTAG, &tBOMTopLine);
    BOM_line_ask_child_lines(tBOMTopLine, &n_children, &children);

    char* hmotnost;
    std::string hmotnostNew;

    AOM_ask_value_string(ItemRev, "tpv4_hmotnost", &hmotnost);

    // kontrola jestli hodnota konèí n " kg"
    if (hasEnding(hmotnost, " kg")) {
        // odebere poslední 3 znaky z hmotnosti
        hmotnostNew = removeLastN(hmotnost, 3);

        AOM_lock(ItemRev);
        AOM_set_value_string(ItemRev, "tpv4_hmotnost", hmotnostNew.c_str());
        AOM_save_with_extensions(ItemRev);

        AOM_unlock(ItemRev);
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

        char* hmotnost;
        std::string hmotnostNew;

        AOM_ask_value_string(ItemRev, "tpv4_hmotnost", &hmotnost);

        // kontrola jestli hodnota konèí n " kg"
        if (hasEnding(hmotnost, " kg")) {
            hmotnostNew = removeLastN(hmotnost, 3);

            // odebere poslední 3 znaky z hmotnosti
            AOM_lock(ItemRev);
            AOM_set_value_string(ItemRev, "tpv4_hmotnost", hmotnostNew.c_str());
            AOM_save_with_extensions(ItemRev);

            AOM_unlock(ItemRev);
        }

        RecursiveBOM(ItemId, RevId);

        MEM_free(ItemId);
        MEM_free(RevId);
    }

    MEM_free(children);
    BOM_save_window(window);
    BOM_close_window(window);

    return 0;
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

int TPV_odstran_kg_TC14(EPM_action_message_t msg)
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

    EPM_ask_root_task(msg.task, &RootTask);
    EPM_ask_attachments(RootTask, EPM_target_attachment, &n_attachments, &attachments);

    for (int i = 0; i < n_attachments; i++)
    {

        POM_class_of_instance(attachments[i], &class_tag);

        // Returns the name of the given class
        POM_name_of_class(class_tag, &class_name);

        if (strcmp(class_name, "TPV4_nak_dilRevision") == 0)
        {
            AOM_ask_value_string(attachments[i], "item_id", &ItemId);
            AOM_ask_value_string(attachments[i], "current_revision_id", &RevId);

            RecursiveBOM(ItemId, RevId);

        }
        if (strcmp(class_name, "TPV4_dilRevision") == 0)
        {
            AOM_ask_value_string(attachments[i], "item_id", &ItemId);
            AOM_ask_value_string(attachments[i], "current_revision_id", &RevId);

            RecursiveBOM(ItemId, RevId);

        }
    }
    return 0;
}