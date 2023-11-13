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
#include <regex>


#define TC_HANDLERS_DEBUG "TC_HANDLERS_DEBUG" 

#define ERROR_CHECK(X) (report_error( __FILE__, __LINE__, #X, (X)));
#define IFERR_REPORT(X) (report_error( __FILE__, __LINE__, #X, (X)));
#define IFERR_RETURN(X) if (IFERR_REPORT(X)) return
#define IFERR_RETURN_IT(X) if (IFERR_REPORT(X)) return X
#define ECHO(X)  printf X; TC_write_syslog X
#define TC_HANDLERS_DEBUG "TC_HANDLERS_DEBUG" 

extern "C" DLLAPI int TPV_kontrola_revize_TC12_register_callbacks();
extern "C" DLLAPI int TPV_kontrola_revize_TC12_init_module(int* decision, va_list args);
int TPV_kontrola_revize_TC12(EPM_action_message_t msg);

extern "C" DLLAPI int TPV_kontrola_revize_TC12_register_callbacks()
{
    printf("Registruji handler-TPV_kontrola_revize_TC12_register_callbacks.dll\n");
    CUSTOM_register_exit("TPV_kontrola_revize_TC12", "USER_init_module", TPV_kontrola_revize_TC12_init_module);

    return ITK_ok;
}

extern "C" DLLAPI int TPV_kontrola_revize_TC12_init_module(int* decision, va_list args)
{
    *decision = ALL_CUSTOMIZATIONS;  // Execute all customizations

    // Registrace action handleru
    int Status = EPM_register_action_handler("TPV_kontrola_revize_TC12", "", TPV_kontrola_revize_TC12);
    if (Status == ITK_ok) printf("Handler pro kontrolu zda se jedna o prvni revizi % s \n", "TPV_kontrola_revize_TC12");


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
    GRM_list_secondary_objects_only(item_revision, relation, &n_secondary_objects, &secondary_objects);

    /* should always be just one */
    item_revision_master_form = secondary_objects[0];

    if (secondary_objects) MEM_free(secondary_objects);
    return item_revision_master_form;
}
const std::pair<int,std::string> findMonth(const std::string& dateString) {
    std::vector<std::string> monthList{"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    std::pair<int, std::string> output;

    for (int i = 0; i < monthList.size(); i++) {
        if (dateString.find(monthList[i]) != std::string::npos) {
            output.first = i + 1;
            output.second = monthList[i];
            return output;
        }
    }
    output.first = -1;
    output.second = "";
    return output; // Return -1, "" if no month is found
}

const std::string modifyMonth(std::string dateString) {
    std::pair<int,std::string> month = findMonth(dateString);

    if (month.first != -1) {
        std::string monthStr = std::to_string(month.first);

        if (monthStr.size() == 1) {
            monthStr = "0" + monthStr;
        }

        dateString = regex_replace(dateString, std::regex(month.second), monthStr);
    }
    return dateString;
}

const std::string modifyDate(char* date) {

    std::string date_string = date;
    if (date_string.size() > 5) {
        date_string.erase(date_string.size() - 5);
        date_string = regex_replace(date_string, std::regex("-"), ". ");
        date_string = modifyMonth(date_string);
    }

    return date_string;
}

int TPV_kontrola_revize_TC12(EPM_action_message_t msg)
{
    ECHO(("************************** zaèátek TPV_kontrola_revize_TC12******************************\n"));
    int n_attachments;
    char
        * class_name,
        * ItemId,
        * RevId,
        * ma4_ApprovalDate,
        * ma4_CheckDate,
        * ma4Approver,
        * ma4Checker,
        * orig_ma4_ApprovalDate,
        * orig_ma4_CheckDate,
        * orig_ma4Approver,
        * orig_ma4Checker;
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

            ECHO(("Revision ID: %s\n", RevId));

            if (strcmp(RevId, "-") == 0) {
                ECHO(("Revize - nalezena\n"));
                // pøepsání atributu do speciálních atributù revize
                tag_t soucasny_master_form = ask_item_revisions_master_form(attachments[i]);
                AOM_lock(attachments[i]);
                AOM_lock(soucasny_master_form);

                AOM_ask_value_string(soucasny_master_form, "ma4_ApprovalDate", &ma4_ApprovalDate);
                AOM_ask_value_string(soucasny_master_form, "ma4_CheckDate", &ma4_CheckDate);
                AOM_ask_value_string(soucasny_master_form, "ma4Approver", &ma4Approver);
                AOM_ask_value_string(soucasny_master_form, "ma4Checker", &ma4Checker);

                //Edit Approval Date and CheckDate
                std::string ma4_ApprovalDate_str = modifyDate(ma4_ApprovalDate);
                std::string ma4_CheckDate_str = modifyDate(ma4_CheckDate);
             

                /*
                std::string string_ma4_ApprovalDate = ma4_ApprovalDate;
                std::string string_ma4_CheckDate = ma4_CheckDate;

                if (string_ma4_ApprovalDate.size() > 5) {
                    string_ma4_ApprovalDate.erase(string_ma4_ApprovalDate.size() - 5);
                    string_ma4_ApprovalDate = regex_replace(string_ma4_ApprovalDate, std::regex("-"), ". ");
                    string_ma4_ApprovalDate = modifyMonth(string_ma4_ApprovalDate);
                    ma4_ApprovalDate = &string_ma4_ApprovalDate[0];
                }
                if (string_ma4_CheckDate.size() > 5) {
                    string_ma4_CheckDate.erase(string_ma4_CheckDate.size() - 5);
                    string_ma4_CheckDate = regex_replace(string_ma4_CheckDate, std::regex("-"), ". ");
                    string_ma4_CheckDate = modifyMonth(string_ma4_CheckDate);
                    ma4_CheckDate = &string_ma4_CheckDate[0];
                }*/

                AOM_set_value_string(soucasny_master_form, "ma4_ApprovalDate", ma4_ApprovalDate_str.c_str());
                AOM_set_value_string(soucasny_master_form, "ma4_CheckDate", ma4_CheckDate_str.c_str());
                AOM_set_value_string(attachments[i], "ma4_ApprovalDate", ma4_ApprovalDate_str.c_str());
                AOM_set_value_string(attachments[i], "ma4_CheckDate", ma4_CheckDate_str.c_str());
                AOM_set_value_string(attachments[i], "ma4Approver", ma4Approver);
                AOM_set_value_string(attachments[i], "ma4Checker", ma4Checker);

                AOM_save(soucasny_master_form);
                AOM_save(attachments[i]);
            }
            else {
                tag_t item,
                    origRev;

                char
                    * ma4_ApprovalDate,
                    * ma4_CheckDate,
                    * ma4Approver,
                    * ma4Checker;

                ITEM_find_item(ItemId, &item);
                ITEM_find_revision(item, "-", &origRev);

                // pøepsání atributu do speciálních atributù revize
                tag_t soucasny_master_form = ask_item_revisions_master_form(attachments[i]);

                AOM_ask_value_string(soucasny_master_form, "ma4_ApprovalDate", &ma4_ApprovalDate);
                AOM_ask_value_string(soucasny_master_form, "ma4_CheckDate", &ma4_CheckDate);
                AOM_ask_value_string(soucasny_master_form, "ma4Approver", &ma4Approver);
                AOM_ask_value_string(soucasny_master_form, "ma4Checker", &ma4Checker);

                ECHO(("ma4_ApprovalDate: %s\nma4_CheckDate: %s\nma4Approver: %s\nma4Checker: %s\n", ma4_ApprovalDate, ma4_CheckDate, ma4Approver, ma4Checker));

                AOM_lock(attachments[i]);

                //Edit Approval Date and CheckDate
                std::string ma4_ApprovalDate_str = modifyDate(ma4_ApprovalDate);
                std::string ma4_CheckDate_str = modifyDate(ma4_CheckDate);

                AOM_set_value_string(attachments[i], "ma4RevApprovalDate", ma4_ApprovalDate_str.c_str());
                AOM_set_value_string(attachments[i], "ma4RevCheckDate", ma4_CheckDate_str.c_str());
                AOM_set_value_string(attachments[i], "ma4RevApprover", ma4Approver);
                AOM_set_value_string(attachments[i], "ma4RevChecker", ma4Checker);

                // pøepsání atributù z nejstarí revize - do master formu souèasné revize

                AOM_ask_value_string(origRev, "ma4_ApprovalDate", &orig_ma4_ApprovalDate);
                AOM_ask_value_string(origRev, "ma4_CheckDate", &orig_ma4_CheckDate);
                AOM_ask_value_string(origRev, "ma4Approver", &orig_ma4Approver);
                AOM_ask_value_string(origRev, "ma4Checker", &orig_ma4Checker);

                ECHO(("Originalni revize:\nma4_ApprovalDate: %s\nma4_CheckDate: %s\nma4Approver: %s\nma4Checker: %s\n", orig_ma4_ApprovalDate, orig_ma4_CheckDate, orig_ma4Approver, orig_ma4Checker));

                AOM_set_value_string(attachments[i], "ma4_ApprovalDate", orig_ma4_ApprovalDate);
                AOM_set_value_string(attachments[i], "ma4_CheckDate", orig_ma4_CheckDate);
                AOM_set_value_string(attachments[i], "ma4Approver", orig_ma4Approver);
                AOM_set_value_string(attachments[i], "ma4Checker", orig_ma4Checker);

                AOM_save(attachments[i]);
            }
        }
    }

    MEM_free(attachments);

    ECHO(("************************** konec TPV_kontrola_revize_TC12 ******************************\n"));
    return 0;
}
