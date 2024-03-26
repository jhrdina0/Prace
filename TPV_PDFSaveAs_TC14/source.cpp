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

extern "C" DLLAPI int TPV_PDFSaveAs_TC14_register_callbacks();
extern "C" DLLAPI int TPV_PDFSaveAs_TC14_init_module(int* decision, va_list args);
int TPV_PDFSaveAs_TC14(EPM_action_message_t msg);

extern "C" DLLAPI int TPV_PDFSaveAs_TC14_register_callbacks()
{
    printf("Registruji handler-TPV_PDFSaveAs_TC14_register_callbacks.dll\n");
    CUSTOM_register_exit("TPV_PDFSaveAs_TC14", "USER_init_module", TPV_PDFSaveAs_TC14_init_module);

    return ITK_ok;
}

extern "C" DLLAPI int TPV_PDFSaveAs_TC14_init_module(int* decision, va_list args)
{
    *decision = ALL_CUSTOMIZATIONS;  // Execute all customizations

    // Registrace action handleru
    int Status = EPM_register_action_handler("TPV_PDFSaveAs_TC14", "", TPV_PDFSaveAs_TC14);
    if (Status == ITK_ok) printf("Handler pro zkopirovani PDF z revize zakaznika do revize dilce a pøipojení logického objektu % s \n", "TPV_PDFSaveAs_TC14");


    return ITK_ok;
}

static tag_t find_dataset(tag_t item_rev) {
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

static void add_logical_object(tag_t pdf) {
    tag_t
        secondary_object_tag,
        relation_type,
        relation;
    GRM_find_relation_type("PDF_vykres_zakaznika", &secondary_object_tag);
    ECHO(("Secondary object tag: %d\n", secondary_object_tag));

    GRM_find_relation_type("Fnd0LogicalObjectTypeRel", &relation_type);
    ECHO(("Relation tag: %d\n", relation_type));

    ECHO(("PDF tag: %d\n", pdf));
    AOM_lock(pdf);
    int error_code = GRM_create_relation(pdf, secondary_object_tag, relation_type, NULLTAG, &relation);
    int error_code2 = GRM_save_relation(relation);
    if (error_code2 != 0) {
        ECHO(("%d Relaci nebylo možné vytvoøit, pravdìpodobnì již existuje.\n", error_code2));
    }
    else {
        ECHO(("Relace vytvoøena.\n"));
    }
    AOM_save_without_extensions(pdf);
}

static tag_t saveAs(tag_t object_to_save, char* new_obj_name,  tag_t add_to_object = 0, char* relation_type_name = (char*)"") {
    int stat;
    tag_t objectType = NULLTAG;
    stat = TCTYPE_ask_object_type(object_to_save, &objectType);
    ECHO(("\nstat: %d\n", stat));
    tag_t saveAsInput = NULLTAG;
    stat = TCTYPE_construct_saveasinput(objectType, &saveAsInput);
    ECHO(("stat: %d\n", stat));
    stat = AOM_set_value_string(saveAsInput, "object_name", new_obj_name);
    ECHO(("stat: %d\n", stat));
    int numDeepCopyData = 0;
    tag_t* deepCopyData = NULL;
    stat = TCTYPE_ask_deepcopydata(object_to_save, TCTYPE_OPERATIONINPUT_SAVEAS, &numDeepCopyData, &deepCopyData);
    ECHO(("stat: %d\n", stat));
    tag_t newObject;
    stat = TCTYPE_saveas_object(object_to_save, saveAsInput, numDeepCopyData, deepCopyData, &newObject);
    ECHO(("stat: %d\n", stat));
    stat = TCTYPE_free_deepcopydata(numDeepCopyData, deepCopyData);
    ECHO(("stat: %d\n", stat));


    if (add_to_object) {

        ECHO(("ADD to relation\n"));
        tag_t relation_type,
            relation;

        stat = ITK_set_bypass(true);
        ECHO(("stat: %d\n", stat));
        stat = AOM_lock(add_to_object);
        ECHO(("stat: %d\n", stat));

        stat = GRM_find_relation_type(relation_type_name, &relation_type);
        ECHO(("stat: %d\n", stat));
        stat = GRM_create_relation(add_to_object, newObject, relation_type, NULL, &relation);
        ECHO(("stat: %d\n", stat));
        stat = GRM_save_relation(relation);
        ECHO(("stat: %d\n", stat));
        stat = AOM_save_without_extensions(add_to_object);
        ECHO(("stat: %d\n", stat));
    }

    return newObject;
}

int TPV_PDFSaveAs_TC14(EPM_action_message_t msg)
{
    ITK_set_bypass(true);
    ECHO(("************************** zaèátek TPV_PDFSaveAs_TC14 ******************************\n"));
    int n_attachments;
    char
        * class_name,
        * object_type;

    tag_t
        RootTask,
        * attachments,
        class_tag,
        v_zakaznikaRevision_tag = 0,
        v_p_dilRevision_tag = 0,
        PDF = 0;

    EPM_ask_root_task(msg.task, &RootTask);
    EPM_ask_attachments(RootTask, EPM_target_attachment, &n_attachments, &attachments);

    for (int i = 0; i < n_attachments; i++)
    {

        POM_class_of_instance(attachments[i], &class_tag);
        AOM_ask_value_string(attachments[i], "object_type", &object_type);

        // Returns the name of the given class
        POM_name_of_class(class_tag, &class_name);

        ECHO(("Class_name: %s\n", class_name));

        if (strcmp(class_name, "TPV4_v_zakaznikaRevision") == 0)
        {
            PDF = find_dataset(attachments[i]);
            if (PDF == 0) {
                ECHO(("PDF Výkres nenalezen ve vykresu zakaznika nenalezen.\n"));
                return 0;
            }
            else {
                v_zakaznikaRevision_tag = attachments[i];
            }
        }
        else if (strcmp(class_name, "TPV4_v_p_dilRevision") == 0) {
            v_p_dilRevision_tag = attachments[i];
        }

    }

    if (v_p_dilRevision_tag and v_zakaznikaRevision_tag and PDF) {
        ECHO(("Vykres zakaznika, vykres dilec a PDF nalezeno, zacinam save as pro PDF...\n"));
        char *item_id,
             *vnejsi_revize;
        AOM_ask_value_string(v_p_dilRevision_tag, "item_id", &item_id);
        AOM_ask_value_string(v_p_dilRevision_tag, "tpv4_vnejsi_revize", &vnejsi_revize);
        std::string new_pdf_name = item_id;
        new_pdf_name += vnejsi_revize;
        TC_write_syslog(new_pdf_name.c_str());
        tag_t newPDF = saveAs(PDF, (char*)new_pdf_name.c_str(), v_p_dilRevision_tag, (char*)"IMAN_specification");
        add_logical_object(newPDF);
    }
    else {
        ECHO(("Spatne vstupni parametry - PDF, vykres zakaznika nebo vykres dilec chybi v Targetech\n"));
    }

    ECHO(("************************** konec TPV_PDFSaveAs_TC14 ******************************\n"));
    return 0;
}