#define  _CRT_SECURE_NO_WARNINGS 1
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
#include <iomanip>
#include <ctime>
#include <qry/qry.h>
#include <ps/ps.h>
#include <string>
#include <nls/nls.h>


#define TC_HANDLERS_DEBUG "TC_HANDLERS_DEBUG" 

extern "C" DLLAPI int TPV_kontrola_csv_TC14_register_callbacks();
extern "C" DLLAPI int TPV_kontrola_csv_TC14_init_module(int* decision, va_list args);
int TPV_kontrola_csv_TC14(EPM_action_message_t msg);

extern "C" DLLAPI int TPV_kontrola_csv_TC14_register_callbacks()
{
    printf("Registruji handler-TPV_kontrola_csv_TC14_register_callbacks.dll\n");
    CUSTOM_register_exit("TPV_kontrola_csv_TC14", "USER_init_module", TPV_kontrola_csv_TC14_init_module);

    return ITK_ok;
}

extern "C" DLLAPI int TPV_kontrola_csv_TC14_init_module(int* decision, va_list args)
{
    *decision = ALL_CUSTOMIZATIONS;  // Execute all customizations

    // Registrace action handleru
    int Status = EPM_register_action_handler("TPV_kontrola_csv_TC4", "", TPV_kontrola_csv_TC14);
    if (Status == ITK_ok) printf("Handler pro kontrolu formatu CSV souboru % s \n", "TPV_kontrola_csv_TC14");


    return ITK_ok;
}

/// reportovani by Gtac

#define ERROR_CHECK(X) (report_error( __FILE__, __LINE__, #X, (X)));
#define IFERR_REPORT(X) (report_error( __FILE__, __LINE__, #X, (X)));
#define IFERR_RETURN(X) if (IFERR_REPORT(X)) return
#define IFERR_RETURN_IT(X) if (IFERR_REPORT(X)) return X
#define ECHO(X)  printf X; TC_write_syslog X

using vectorArray = std::vector<std::vector<std::string>>;

std::vector<std::string> splitString(const std::string& line, char delimiter) {
    std::vector<std::string> tokens;
    std::istringstream tokenStream(line);
    std::string token;

    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

vectorArray readCSV(const std::string& filename) {
    // funkce na nahrani csv souboru do "2D listu" 
    std::vector<std::vector<std::string>> data;
    std::ifstream file(filename);
    std::string line;

    while (std::getline(file, line)) {
        std::vector<std::string> row = splitString(line, ';');
        data.push_back(row);
    }

    file.close();

    return data;
}

bool is_number(const std::string& s)
{
    return !s.empty() && std::find_if(s.begin(),
        s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
}

bool in_array(const std::string& value)
{
    std::vector<std::string> array = { "V", "D", "M" };
    return std::find(array.begin(), array.end(), value) != array.end();
}

int kontrola(std::string csvPath, tag_t RootTask, tag_t dataset) {
    std::string PDFname;
    std::string error_text_string = "\nCSV: ";
    char* object_name;
    AOM_ask_value_string(dataset, "object_name", &object_name);
    
    error_text_string += object_name;
    error_text_string += "\n";
    vectorArray csvData = readCSV(csvPath);

    for (int i = 0; i < csvData.size(); i++) {
        try {
            if (csvData[i].size() > 7) {
                ECHO(("505\n"));
                error_text_string += "\nRadek " + std::to_string(i + 1) + " ma prilis mnoho sloupcu: " + std::to_string(csvData[i].size()) + ", sloupcu musi byt 6 nebo 7";
                throw 505;
            }
            if (csvData[i].size() < 6) {
                ECHO(("506\n"));
                error_text_string += "\nRadek " + std::to_string(i + 1) + " ma prilis malo sloupcu: " + std::to_string(csvData[i].size()) + ", sloupcu musi byt 6 nebo 7";
                throw 506;
            }
            for (int j = 0; j < csvData[i].size(); j++) {
                if (csvData[i][j] == "") {
                    ECHO(("507\n"));
                    error_text_string += "\nNa Radku " + std::to_string(i + 1) + " v sloupci " + std::to_string(j + 1) + " (" + csvData[0][j] + ") nalezena prazdna hodnota.";
                    throw 507;
                }
            }
            if (not is_number(csvData[i][0]) and i != 0) {
                ECHO(("508\n"));
                error_text_string += "\nNa Radku " + std::to_string(i + 1) + " v sloupci 1 (" + csvData[0][0] + ") nalezena nepovolena hodnota: " + csvData[i][0] + ". \nTento sloupec je vyhrazen pro uroven.\nHodnota musi byt cele cislo.";
                throw 508;
            }
            if ((csvData[i][0] == "0" and i != 1) or (csvData[i][0] != "0" and i == 1)) {
                ECHO(("509\n"));
                error_text_string += "\nNa Radku " + std::to_string(i + 1) + " v sloupci 1 (" + csvData[0][0] + ") nalezena nepovolena hodnota: " + csvData[i][0] + ". \nUroven 0 musi byt pouze na radku 2 v sloupci 1.";
                throw 509;
            }

            if (not in_array(csvData[i][1]) and i != 0) {
                ECHO(("510\n"));
                error_text_string += "Na Radku " + std::to_string(i + 1) + " v sloupci 2 (" + csvData[0][1] + ") nalezena nepovolena hodnota: " + csvData[i][1] + ". \nTento sloupec je vyhrazen pro druh itemu, povolene hodnoty jsou:\nD,V -> TPV4_Vyrobek\nM -> TPV4_nakupovany_dil";
                throw 510;
            }

            if (not is_number(csvData[i][4]) and i != 0 or csvData[i][4] == "0") {
                ECHO(("511\n"));
                error_text_string += "Na Radku " + std::to_string(i + 1) + " v sloupci 5 (" + csvData[0][4] + ") nalezena nepovolena hodnota: " + csvData[i][4] + ". \nTento sloupec je vyhrazen pro mnozstvi, musi tedy obsahovat ciselnou hodnotu vetsi nez 0.";
                throw 511;
            }
        }
        catch (...) {
            ECHO(("ERROR, spatny format CSV!\n"));
            //C:\SPLM\TC\lang\textserver\cs_CZ\ue_errors.xml
            //EMH_clear_errors();
            error_text_string += "\nSpravny format sloupcu v CSV: Uroven, Druh, Komponenta(ID), Nazev, Mnozstvi, Merna jednotka";
            error_text_string += "\nCSV opravte a vlozte ho znovu do cile.";

            char* utf8_string;
            NLS_convert_from_platform_to_utf8_encoding(error_text_string.c_str(), &utf8_string);
            EMH_store_error_s1(2, 919002, utf8_string);
            remove(csvPath.c_str());

            EPM_remove_attachments(RootTask, 1, &dataset);
            AE_delete_all_dataset_revs(dataset);
            return 919002;
        }
    }
    ECHO(("Spravny format CSV.\n"));

    remove(csvPath.c_str());
    return 0;
}

int TPV_kontrola_csv_TC14(EPM_action_message_t msg)
{
    int n_attachments;
    char
        * class_name;
    tag_t
        RootTask,
        * attachments,
        class_tag,
        dataset;

    time_t rawtime;
    struct tm* timeinfo;
    char buffer[80];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, sizeof(buffer), "%d-%m-%Y-%H-%M-%S", timeinfo);
    std::string strTime(buffer);

    std::string csvPath;
    bool exportSucces = false,
        addRelation = false;

    EPM_ask_root_task(msg.task, &RootTask);
    EPM_ask_attachments(RootTask, EPM_target_attachment, &n_attachments, &attachments);
    ECHO(("%d %d \n", __LINE__, n_attachments));

    for (int i = 0; i < n_attachments; i++)
    {
        POM_class_of_instance(attachments[i], &class_tag);
        // Returns the name of the given class
        POM_name_of_class(class_tag, &class_name);

        if (strcmp(class_name, "Dataset") == 0) {
            char* object_type;
            AOM_ask_value_string(attachments[i], "object_type", &object_type);

            if (strcmp(object_type, "Text") == 0) {
                csvPath = "C:\\ProgramData\\TPV_CSV2BOM_TCtemp" + strTime + "-" + std::to_string(i) + ".csv";
                AE_export_named_ref(attachments[i], "Text", csvPath.c_str());
                if (kontrola(csvPath, RootTask, attachments[i]) != 0) {
                    return 919002;
                }
                exportSucces = true;
            }
        }
        ECHO(("class_name: %s \n", class_name));
    }

    if (not exportSucces) {
        ECHO(("Failed to export dataset. \n"));
        //C:\SPLM\TC\lang\textserver\cs_CZ\ue_errors.xml

        EMH_store_error_s1(2, 919002, "\nCSV soubor nebyl v cili nalezen");
        return 919002;
    }

    return 0;
}
