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

#define TC_HANDLERS_DEBUG "TC_HANDLERS_DEBUG" 

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

tag_t find_itemRevision(const std::string& InputAttrValues, const std::string& RevId) {
    // Vyhledání položek
    const char* AttrNames[1] = { ITEM_ITEM_ID_PROP };
    const char* AttrValues[1] = { InputAttrValues.c_str() };
    int ItemsCount = 0;
    //tag_t* Items = NULLTAG;
    tag_t* Items;
    ITEM_find_items_by_key_attributes(1, AttrNames, AttrValues, &ItemsCount, &Items);

    tag_t Rev;
    ITEM_find_revision(Items[0], RevId.c_str(), &Rev);

    MEM_free(Items);
    //vrátí tag revize Itemu
    return Rev;
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

std::vector<std::vector<std::string>> RecursiveBOM(const std::string& InputAttrValues, const std::string& RevId, std::vector<std::vector<std::string>> csv)
{
    int n_children;
    tag_t
        ItemRev,
        window,
        tBOMTopLine,
        * children;

    ItemRev = find_itemRevision(InputAttrValues, RevId);
    BOM_create_window(&window);
    BOM_set_window_top_line(window, NULLTAG, ItemRev, NULLTAG, &tBOMTopLine);

    BOM_line_ask_child_lines(tBOMTopLine, &n_children, &children);

    // Iterate over chlidren (if there are any) and call recursiveBOM function on them
    for (int i = 0; i < n_children; i++)
        {
            const std::string ParentId = InputAttrValues;
            char 
                * ItemId,
                * RevId,        
                * ItemQuantity,
                * seqNo;
            std::vector<std::string> csv_row;

            ITEM_ask_rev_id2(ask_item_revision_from_bom_line(children[i]), &RevId);

            AOM_ask_value_string(children[i], "bl_item_item_id", &ItemId);
            AOM_ask_value_string(children[i], "bl_rev_item_revision_id", &RevId);
            AOM_ask_value_string(children[i], "bl_quantity",&ItemQuantity);
            AOM_ask_value_string(children[i], "bl_sequence_no", &seqNo);

            // load all required values into a row
            csv_row.push_back(ParentId);
            csv_row.push_back(ItemId);
            csv_row.push_back(RevId);
            csv_row.push_back(ItemQuantity);
            csv_row.push_back(seqNo);

            // push new row into the csv
            csv.push_back(csv_row);

            csv = RecursiveBOM(ItemId, RevId, csv);

            MEM_free(ItemId);
            MEM_free(RevId);
            MEM_free(ItemQuantity);
            MEM_free(seqNo);
        }

    MEM_free(children);

    BOM_save_window(window);
    BOM_close_window(window);

    return csv;
}

int createCSV(std::vector<std::vector<std::string>> csv)
{
    std::ofstream file;
    file.open("C:\\SPLM\\Apps\\Export\\Export.csv");
    file << "PARENT ITEM ID, ITEM ID, ITEM REV ID, ITEM QUANTITY, FIND NO\n";
    for (int i = 0; i < csv.size(); i++) {
        int j = 0;
        for (j; j < 5; j++) {
            file << csv[i][j];
            file << ",";
        }
        file << "\n";
    }

    file.close();
    return 0;
}

int TPV_BOM2CSV_TC14(EPM_action_message_t msg)
{
    std::vector<std::vector<std::string>> csv;

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

        if (strcmp(class_name, "TPV4_hut_matRevision") == 0)
        {
            
            AOM_ask_value_string(attachments[i], "item_id", &ItemId);
            AOM_ask_value_string(attachments[i], "current_revision_id", &RevId);

            csv = RecursiveBOM(ItemId, RevId, csv);

            createCSV(csv);
        }
    }
    return 0;
}