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

tag_t find_dataset(tag_t item_rev) {
    tag_t 
        relationType,
        type_tag,
        * specs;
    int n_specs;
    char
        * type_name,
        * dataset_name;

    GRM_find_relation_type("IMAN_specification", &relationType);
    GRM_list_secondary_objects_only(item_rev, relationType, &n_specs, &specs);

    ECHO(("Amount: %d\n", n_specs));

    for (int i = 0; i < n_specs; i++) {
        TCTYPE_ask_object_type(specs[i], &type_tag);
        TCTYPE_ask_name2(type_tag, &type_name);
        AOM_ask_value_string(specs[i], "object_type", &dataset_name);
        if (strcmp(dataset_name, "PDF") == 0) {
            ECHO(("Found dataset PDF\n"));
            return specs[i];
        }
        ECHO(("Dataset name: %s\n", dataset_name));
    }
    return 0;
}

void add_logical_object(tag_t pdf) {
    tag_t 
        secondary_object_tag,
        relation_type,
        relation;
    GRM_find_relation_type("PDF_Approve_Info", &secondary_object_tag);
    ECHO(("Secondary object tag: %d\n", secondary_object_tag));

    GRM_find_relation_type("Fnd0LogicalObjectTypeRel", &relation_type);
    ECHO(("Relation tag: %d\n", relation_type));

    ECHO(("PDF tag: %d\n", pdf));
    AOM_lock(pdf);
    int error_code = GRM_create_relation(pdf, secondary_object_tag, relation_type, NULLTAG, &relation);
    int error_code2 = GRM_save_relation(relation);
    if (error_code2 != 0) {
        ECHO(("%d Relaci nebylo možné vytvořit, pravděpodobně již existuje.\n", error_code2));
    }
    else {
        ECHO(("Relace vytvořena.\n"));
    }
    AOM_save(pdf);
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

        if (strcmp(class_name, "MA4DesignPartRevision") == 0)
        {
            tag_t PDF = find_dataset(attachments[i]);
            if (PDF == 0) {
                ECHO(("PDF Výkres nenalezen.\n"));
            }
            else {
                add_logical_object(PDF);
            }
        }
    }

    ECHO(("************************** konec TPV_vloz_logicky_objekt_TC12 ******************************\n"));
    return 0;
}
