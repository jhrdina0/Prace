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

#define ERROR_CHECK(X) (report_error( __FILE__, __LINE__, #X, (X)));
#define IFERR_REPORT(X) (report_error( __FILE__, __LINE__, #X, (X)));
#define IFERR_RETURN(X) if (IFERR_REPORT(X)) return
#define IFERR_RETURN_IT(X) if (IFERR_REPORT(X)) return X
#define ECHO(X)  printf X; TC_write_syslog X
#define TC_HANDLERS_DEBUG "TC_HANDLERS_DEBUG" 

extern "C" DLLAPI int TPV_vloz_logicky_objekt_TC12_register_callbacks();
extern "C" DLLAPI int TPV_vloz_logicky_objekt_TC12_init_module(int* decision, va_list args);
int TPV_vloz_logicky_objekt_TC12(EPM_action_message_t msg);

extern "C" DLLAPI int TPV_vloz_logicky_objekt_TC12_register_callbacks()
{
    printf("Registruji handler-TPV_vloz_logicky_objekt_TC12_register_callbacks.dll\n");
    CUSTOM_register_exit("TPV_vloz_logicky_objekt_TC12", "USER_init_module", TPV_vloz_logicky_objekt_TC12_init_module);

    return ITK_ok;
}

extern "C" DLLAPI int TPV_vloz_logicky_objekt_TC12_init_module(int* decision, va_list args)
{
    *decision = ALL_CUSTOMIZATIONS;  // Execute all customizations

    // Registrace action handleru
    int Status = EPM_register_action_handler("TPV_vloz_logicky_objekt_TC12", "", TPV_vloz_logicky_objekt_TC12);
    if (Status == ITK_ok) printf("Handler pro vlozeni logického objektu do PDF % s \n", "TPV_vloz_logicky_objekt_TC12");


    return ITK_ok;
}

int TPV_vloz_logicky_objekt_TC12(EPM_action_message_t msg)
{
    ECHO(("************************** začátek TPV_vloz_logicky_objekt_TC12 ******************************\n"));
    int n_attachments;
    char
        * class_name,
        * dataset_name,
        * object_type;

    tag_t
        RootTask,
        * attachments,
        class_tag,
        relation_type,
        relation;

    EPM_ask_root_task(msg.task, &RootTask);
    EPM_ask_attachments(RootTask, EPM_target_attachment, &n_attachments, &attachments);

    for (int i = 0; i < n_attachments; i++)
    {

        POM_class_of_instance(attachments[i], &class_tag);
        AOM_ask_value_string(attachments[i], "object_type", &object_type);

        // Returns the name of the given class
        POM_name_of_class(class_tag, &class_name);

        ECHO(("Class_name: %s\n", class_name));

        if (strcmp(class_name, "Dataset") == 0)
        {
            AOM_ask_value_string(attachments[i], "object_type", &object_type);
            ECHO(("Object_type: %s\n", object_type));
            if (strcmp(object_type, "PDF") == 0) {

                AOM_ask_value_string(attachments[i], "object_name", &dataset_name);
                ECHO(("Dataset name: %s\n", dataset_name));

                tag_t secondary_object_tag;
                GRM_find_relation_type("PDF_Approve_Info", &secondary_object_tag);
                ECHO(("Secondary object tag: %d\n", secondary_object_tag));

                GRM_find_relation_type("Fnd0LogicalObjectTypeRel", &relation_type);
                ECHO(("Relation tag: %d\n", relation_type));

                AOM_lock(attachments[i]);
                int error_code = GRM_create_relation(attachments[i], secondary_object_tag, relation_type, NULLTAG, &relation);
                int error_code2 = GRM_save_relation(relation);
                if (error_code2 != 0) {
                    ECHO(("%d Relace jiz existuje !\n", error_code2));
                }
                else {
                    ECHO(("Relace vytvořena.\n"));
                }
                AOM_save(attachments[i]);
            }
        }
    }

    ECHO(("************************** konec TPV_vloz_logicky_objekt_TC12 ******************************\n"));
    return 0;
}
