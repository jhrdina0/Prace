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
#include <qry/qry.h>
#include <tccore\grm.h>

#define TC_HANDLERS_DEBUG "TC_HANDLERS_DEBUG" 

#define ERROR_CHECK(X) (report_error( __FILE__, __LINE__, #X, (X)));
#define IFERR_REPORT(X) (report_error( __FILE__, __LINE__, #X, (X)));
#define IFERR_RETURN(X) if (IFERR_REPORT(X)) return
#define IFERR_RETURN_IT(X) if (IFERR_REPORT(X)) return X
#define ECHO(X)  printf X; TC_write_syslog X
#define TC_HANDLERS_DEBUG "TC_HANDLERS_DEBUG" 

extern "C" DLLAPI int TPV_copyPV_TC14_register_callbacks();
extern "C" DLLAPI int TPV_copyPV_TC14_init_module(int* decision, va_list args);
int TPV_copyPV_TC14(EPM_action_message_t msg);

extern "C" DLLAPI int TPV_copyPV_TC14_register_callbacks()
{
    printf("Registruji handler-TPV_copyPV_TC14_register_callbacks.dll\n");
    CUSTOM_register_exit("TPV_copyPV_TC14", "USER_init_module", TPV_copyPV_TC14_init_module);

    return ITK_ok;
}

extern "C" DLLAPI int TPV_copyPV_TC14_init_module(int* decision, va_list args)
{
    *decision = ALL_CUSTOMIZATIONS;  // Execute all customizations

    // Registrace action handleru
    int Status = EPM_register_action_handler("TPV_copyPV_TC14", "", TPV_copyPV_TC14);
    if (Status == ITK_ok) printf("Handler pro zkopirovani PV % s \n", "TPV_copyPV_TC14");


    return ITK_ok;
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
        if (strcmp("excelNazev", Flag) == 0 && Value != nullptr)
        {
            atribut = Value;
        }
        MEM_free(Flag);
        MEM_free(Value);
    }

    return atribut;
}

int TPV_copyPV_TC14(EPM_action_message_t msg)
{

    int n_attachments;
    char
        * class_name,
        * ItemId,
        * RevId,
        *item_name;
    tag_t
        RootTask,
        * attachments,
        class_tag,
        query;

    int n_found;
    tag_t dataset;

    ECHO(("**********ZACATEK copyPV tc14****************\n"));
  
    std::string excelNazev = read_properties(msg);
    ECHO(("nazev excelu: %s\n", excelNazev));

    //char* entries[1] = { (char*)("Název") };
    //char* values[1] = { const_cast<char*>(excelNazev.c_str()) };

    //AE_find_dataset2((char*)("Název"), &dataset);
    //QRY_find2("PV", &query);
    //QRY_execute(query, 1, entries, values, &n_found, &results);

    //ECHO(("n_found: %d \n", n_found));

    AE_find_dataset2((char*)(excelNazev.c_str()), &dataset);
    ECHO(("Dataset tag: %d \n", dataset));

    EPM_ask_root_task(msg.task, &RootTask);
    EPM_ask_attachments(RootTask, EPM_target_attachment, &n_attachments, &attachments);

    //AE_export_named_ref(dataset, "excel", "C:\Temp");
    for (int i = 0; i < n_attachments; i++)
    {
        POM_class_of_instance(attachments[i], &class_tag);

        // Returns the name of the given class
        POM_name_of_class(class_tag, &class_name);

        if (strcmp(class_name, "TPV4_PoptavkaRevision") == 0) {
            POM_class_of_instance(attachments[0], &class_tag);

            // Returns the name of the given class
            POM_name_of_class(class_tag, &class_name);

            AOM_ask_value_string(attachments[i], "item_id", &ItemId);
            AOM_ask_value_string(attachments[i], "current_revision_id", &RevId);
            AOM_ask_value_string(attachments[i], "current_name", &item_name);
            // AE_export_named_ref(dataset, "excel", "C:\Temp\pv.xlsx");

            // vytvoreni noveho datasetu
            tag_t new_dataset;
            std::string item_id = ItemId;
            std::string new_name = "FV-" + item_id;
            AE_copy_dataset_with_id(dataset, new_name.c_str(), "1", "1", &new_dataset);

            tag_t relation_type,
                relation;
            GRM_find_relation_type("TPV4_Prezkum_poptavky", &relation_type);
            GRM_create_relation(attachments[i], new_dataset, relation_type, NULLTAG, &relation);
            GRM_save_relation(relation);
            ECHO(("ITEM ID: %s ITEM REV ID: %s\n", ItemId, RevId));
        }
    }

    ECHO(("**********KONEC copyPV tc14****************\n"));
    return 0;

}
