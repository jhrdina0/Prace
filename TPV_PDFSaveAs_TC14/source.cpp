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
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <tccore/uom.h>
#include <ae\ae.h>
#include <tccore\grm.h>
#include <nls/nls.h>

#include <filesystem>


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
    if (Status == ITK_ok) printf("Handler pro zkopirovani PDF z revize zakaznika do revize dilce a připojení logického objektu % s \n", "TPV_PDFSaveAs_TC14");


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
        ECHO(("%d Relaci nebylo možné vytvořit, pravděpodobně již existuje.\n", error_code2));
    }
    else {
        ECHO(("Relace vytvořena.\n"));
    }
    AOM_save_without_extensions(pdf);
}

static tag_t saveAs(tag_t object_to_save, std::string new_obj_name,  tag_t add_to_object = 0, std::string relation_type_name = "") {
    int stat,
        numDeepCopyData;
    tag_t objectType,
        saveAsInput,
        * deepCopyData,
        newObject;

    stat = TCTYPE_ask_object_type(object_to_save, &objectType);
    ECHO(("\nstat: %d\n", stat));
    stat = TCTYPE_construct_saveasinput(objectType, &saveAsInput);
    ECHO(("stat: %d\n", stat));
    stat = AOM_set_value_string(saveAsInput, "object_name", new_obj_name.c_str());
    ECHO(("stat: %d\n", stat));
    stat = TCTYPE_ask_deepcopydata(object_to_save, TCTYPE_OPERATIONINPUT_SAVEAS, &numDeepCopyData, &deepCopyData);
    ECHO(("stat: %d\n", stat));
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

        stat = GRM_find_relation_type(relation_type_name.c_str(), &relation_type);
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

void exec_command(const char* command) {
    FILE* fp;
    char buffer[1024];

    // Open a pipe to the command
    fp = _popen(command, "r");

    if (fp == NULL) {
        printf("Failed to execute command.\n");
        return;
    }

    // Read output from the command line by line
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        ECHO(("%s", buffer));
    }

    // Close the pipe
    _pclose(fp);
}

int TPV_PDFSaveAs_TC14(EPM_action_message_t msg)
{
    ITK_set_bypass(true);
    ECHO(("************************** začátek TPV_PDFSaveAs_TC14 ******************************\n"));
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

    std::vector<tag_t> PDFs;

    ECHO(("Počet attachmentů: %d\n", n_attachments));

    for (int i = 0; i < n_attachments; i++)
    {

        POM_class_of_instance(attachments[i], &class_tag);
        AOM_ask_value_string(attachments[i], "object_type", &object_type);

        // Returns the name of the given class
        POM_name_of_class(class_tag, &class_name);

        ECHO(("Class_name: %s\n", class_name));

        if (strcmp(class_name, "Dataset") == 0)
        {
            char* object_type;
            AOM_ask_value_string(attachments[i], "object_type", &object_type);
            if (strcmp(object_type, "PDF") == 0) {
                TC_write_syslog("PDF Found !\n");
                PDFs.push_back(attachments[i]);
            }
        }
        else if (strcmp(class_name, "TPV4_v_p_dilRevision") == 0) {
            v_p_dilRevision_tag = attachments[i];
        }
    }

    MEM_free(attachments);

    char* PDF_name,
        * v_p_dilID,
        * v_p_dilRevID;

    tag_t dataset_type,
          new_dataset,
          relation_type,
          relation;

    if (v_p_dilRevision_tag) {
        AOM_ask_value_string(v_p_dilRevision_tag, "item_id", &v_p_dilID);
        AOM_ask_value_string(v_p_dilRevision_tag, "tpv4_vnejsi_revize", &v_p_dilRevID);

        std::string text = v_p_dilID;
        text += " ";
        text += v_p_dilRevID;

        AOM_lock(v_p_dilRevision_tag);

        GRM_find_relation_type("TPV4_Export_do_IFS", &relation_type);

        for (int i = 0; i < PDFs.size(); i++) {
            ECHO(("PDF save as start...\n"));
            AOM_ask_value_string(PDFs[i], "object_name", &PDF_name);


            std::string path = "C:\\SPLM\\Apps\\TiskPDF\\pdf\\";
            path += PDF_name;

            int ITK_OK = AE_export_named_ref(PDFs[i], "PDF_Reference", path.c_str());
            ECHO(("Export dataset OK: %d\n", ITK_OK));
            
            std::string cmd = "C:\\SPLM\\Apps\\TiskPDF\\callPDFMargin.bat ";
            cmd += "\"";
            cmd += text;
            cmd += "\" ";
            cmd += "\"";
            char* utf8_str;
            ITK_OK = NLS_convert_from_utf8_to_platform_encoding(path.c_str(), &utf8_str);
            cmd += utf8_str;
            cmd += "\"";

            TC_write_syslog(cmd.c_str());


            exec_command(cmd.c_str());

            AE_find_datasettype2("PDF", &dataset_type);
            ITK_OK = AE_create_dataset_with_id(dataset_type, PDF_name, "", "", "", &new_dataset);
            ECHO(("\nCreate dataset OK: %d\n", ITK_OK));

            AOM_lock(new_dataset);
            ITK_OK = AE_import_named_ref(new_dataset, "PDF_Reference", path.c_str(), "", SS_BINARY);
            ECHO(("Import named ref OK: %d\n", ITK_OK));

            if (std::filesystem::remove(utf8_str)) {
                ECHO(("file path deleted.\n"));
            }
            else {
                ECHO(("DeleteFileA failed\n"));
            }

            AOM_save_with_extensions(new_dataset);
            PDF = new_dataset;

            GRM_find_relation_type("TPV4_Export_do_IFS", &relation_type);
            ITK_OK = GRM_create_relation(v_p_dilRevision_tag, PDF, relation_type, NULL, &relation);
            ECHO(("GRM_create_relation ITK_OK: %d\n", ITK_OK));
            ECHO(("New relation tag: %d\n", relation));

            GRM_save_relation(relation);
        }
    }

    AOM_save_with_extensions(v_p_dilRevision_tag);

    ECHO(("************************** konec TPV_PDFSaveAs_TC14 ******************************\n"));
    return 0;
}
