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

extern "C" DLLAPI int TPV_copy_attributes_TC12_register_callbacks();
extern "C" DLLAPI int TPV_copy_attributes_TC12_init_module(int* decision, va_list args);
int TPV_copy_attributes_TC12(EPM_action_message_t msg);

extern "C" DLLAPI int TPV_copy_attributes_TC12_register_callbacks()
{
    printf("Registruji handler-TPV_copy_attributes_TC12_register_callbacks.dll\n");
    CUSTOM_register_exit("TPV_copy_attributes_TC12", "USER_init_module", TPV_copy_attributes_TC12_init_module);

    return ITK_ok;
}

extern "C" DLLAPI int TPV_copy_attributes_TC12_init_module(int* decision, va_list args)
{
    *decision = ALL_CUSTOMIZATIONS;  // Execute all customizations

    // Registrace action handleru
    int Status = EPM_register_action_handler("TPV_copy_attributes_TC12", "", TPV_copy_attributes_TC12);
    if (Status == ITK_ok) printf("Handler pro zkopirovani atributu z formu na Revizi % s \n", "TPV_copy_attributes_TC12");


    return ITK_ok;
}

static tag_t ask_item_revisions_master_form(tag_t item_revision)
{
    int
        n_secondary_objects = 0;
    tag_t
        relation = NULLTAG,
        * secondary_objects = NULL,
        item_revision_master_form = NULLTAG;

    GRM_find_relation_type("IMAN_master_form", &relation);
    GRM_list_secondary_objects_only(item_revision, relation,&n_secondary_objects, &secondary_objects);

    /* should always be just one */
    item_revision_master_form = secondary_objects[0];

    if (secondary_objects) MEM_free(secondary_objects);
    return item_revision_master_form;
}

int TPV_copy_attributes_TC12(EPM_action_message_t msg)
{
    ECHO(("************************** začátek TPV_COPY_ATTRIBUTES_TC12******************************\n"));
    int n_attachments;
    char
        * class_name,
        * ItemId,
        * RevId,
        * approval_date,
        * approver,
        * check_date,
        * checker;
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

        if (strcmp(class_name, "MA4DesignPartRevision") == 0)
        {
            ECHO(("Design part found!\n"));
            AOM_ask_value_string(attachments[i], "item_id", &ItemId);
            AOM_ask_value_string(attachments[i], "current_revision_id", &RevId);

            tag_t master_form = ask_item_revisions_master_form(attachments[i]);

            ECHO(("master_form_tag: %d\n", master_form));

            // načtení atributů
            AOM_ask_value_string(master_form, "ma4_ApprovalDate", &approval_date);
            AOM_ask_value_string(master_form, "ma4Approver", &approver);
            AOM_ask_value_string(master_form, "ma4_CheckDate", &check_date);
            AOM_ask_value_string(master_form, "ma4Checker", &checker);

            ECHO(("Approval date: %s, Approver: %s, Check date: %s, Checker: %s\n", approval_date, approver, check_date, checker));

            // zapsání atributů do revize
            AOM_lock(attachments[i]);
            AOM_set_value_string(attachments[i], "ma4_ApprovalDate", approval_date);
            AOM_set_value_string(attachments[i], "ma4Approver", approver);
            AOM_set_value_string(attachments[i], "ma4_CheckDate", check_date);
            AOM_set_value_string(attachments[i], "ma4Checker", checker);
            AOM_save(attachments[i]);

            // smazání atributů z Master Formu
            AOM_lock(master_form);
            AOM_set_value_string(master_form, "ma4_ApprovalDate", "");
            AOM_set_value_string(master_form, "ma4Approver", "");
            AOM_set_value_string(master_form, "ma4_CheckDate", "");
            AOM_set_value_string(master_form, "ma4Checker", "");
            AOM_save(master_form);
            return 0;
        }
    }

    ECHO(("************************** konec TPV_COPY_ATTRIBUTES_TC12******************************\n"));
    return 0;
}