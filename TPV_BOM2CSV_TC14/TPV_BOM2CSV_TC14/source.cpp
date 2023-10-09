#define _CRT_SECURE_NO_WARNINGS
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
#include <ctime>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <ae/ae.h>
#include <sa/tcvolume.h>

#define TC_HANDLERS_DEBUG "TC_HANDLERS_DEBUG" 
#define ECHO(X)  printf X; TC_write_syslog X; //(LogErr(#X,"TC2ERP_err.log",__LINE__)); 
using vectorArray = std::vector<std::vector<std::string>>;
boolean definefirstline;
char callBat[100] = "0";

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
		*rev_id = NULL;

	get_string(bom_line, "bl_item_item_id", &item_id);
	get_string(bom_line, "bl_rev_item_revision_id", &rev_id);
	ITEM_find_rev(item_id, rev_id, &item_revision);

	if (item_id) MEM_free(item_id);
	if (rev_id) MEM_free(rev_id);
	return item_revision;
}


std::string convertDateToString(const date_t& date) {
	int month = date.month;
	int day = date.day;
	int hour = date.hour;
	int minute = date.minute;
	int second = date.second;
	std::string s_month,
		s_day,
		s_hour,
		s_minute,
		s_second;

	if (month < 10) s_month="0"+std::to_string(date.month);
	else s_month = std::to_string(date.month);
	
	if (day < 10)s_day = "0" + std::to_string(date.day);
	else s_day = std::to_string(date.day);

	if (hour < 10)s_hour = "0" + std::to_string(date.hour);
	else s_hour = std::to_string(date.hour);

	if (minute < 10)s_minute = "0" + std::to_string(date.minute);
	else s_minute = std::to_string(date.minute);

	if (second < 10)s_second = "0" + std::to_string(date.second);
	else s_second = std::to_string(date.second);
	std::string dateString = s_day + "-" +
		s_month + "-" +
		std::to_string(date.year) + " " +
		s_hour + ":" +
		s_minute+ ":" +
		s_second;

	return dateString;
}

std::string exec(const char* cmd) {
	std::array<char, 128> buffer;
	std::string result;
	std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
	if (!pipe) {
		throw std::runtime_error("popen() failed!");
		ECHO(("L:%d \n", __LINE__));
	}
	while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
		result += buffer.data();
		ECHO(("L:%d \n", __LINE__));
	}
	return result;
}

int CountInRelation(tag_t Child, std::string Relation, tag_t** primary_obj)
{
	int Count = 0;
	int returnCount = 0;
	tag_t * primary_list = NULLTAG;
	tag_t relation_type;
	ECHO(("L:%d find relation %s \n", __LINE__, Relation.c_str()));
	int err = GRM_find_relation_type(Relation.c_str(), &relation_type);
	if (err != ITK_ok) { ECHO(("Problem err %d \n", err)); }
	ECHO(("L:%d find relation %d \n", __LINE__, relation_type));
	//err = GRM_list_primary_objects_only(Child, relation_type, &Count, &primary_list);
	err = GRM_list_secondary_objects_only(Child, relation_type, &Count, &primary_list);
	ECHO(("count %d \n", Count));
	if (err != ITK_ok) { ECHO(("Problem err %d \n", err)); }
	//GetName_rev(primary_list[0]);
	if (Count == 1)
	{
		*primary_obj[0] = primary_list[0];
		++returnCount;
	}
	else if (Count > 1)
	{
		for (int i = 0;i < Count;i++)
		{
			logical latest = false;
			ITEM_rev_sequence_is_latest(primary_list[i], &latest);
			if (latest)
			{
				char *Type;
				WSOM_ask_object_type2(primary_list[i], &Type);//Returns the object type of the specified WorkspaceObject.

				*primary_obj[returnCount] = primary_list[i];
				++returnCount;//kdyz neni zakomentovaný blok níže toto smazat
			   /*if (strcmp(Type,"H4_LAKRevision")==0 ||strcmp(Type,"H4_NPVDRevision")==0)
			   {


				   if(primary_list) MEM_free(primary_list);
				   return 1;
			   }*/
			}
		}
	}
	//printf ("secondary list [0] %d \n",*secondary_list);
	//if(Count>0)
	//Add_latets_rev_TP_ToRef( RootTask,secondary_list[0], Count);
	if (primary_list) MEM_free(primary_list);
	return returnCount;
}
char* ask_imanfile_path(tag_t fileTag )
{
	char *path;
	int
		machine_type;
	tag_t
		volume;

	IMF_ask_volume(fileTag, &volume);
	VM_ask_machine_type(volume, &machine_type);
	IMF_ask_file_pathname2(fileTag, machine_type, &path);
	return path;
}

std::string readElement(tag_t Tag, std::string property_name) {
	PROP_value_type_t valtype;
	char* valtype_n;
	AOM_ask_value_type(Tag, property_name.c_str(), &valtype, &valtype_n);
	MEM_free(valtype_n);
	if (valtype == PROP_char) {
		char value;
		AOM_ask_value_char(Tag, property_name.c_str(), &value);
		return std::string(1, value);
	}
	else if (valtype == PROP_date) {
		date_t value;
		AOM_ask_value_date(Tag, property_name.c_str(), &value);
		return convertDateToString(value);
	}
	else if (valtype == PROP_double) {
		double value;
		AOM_ask_value_double(Tag, property_name.c_str(), &value);
		return std::to_string(value);
	}
	else if (valtype == PROP_float) {
		// Handle float type
	}
	else if (valtype == PROP_int) {
		int value;
		AOM_ask_value_int(Tag, property_name.c_str(), &value);
		return std::to_string(value);
	}
	else if (valtype == PROP_logical) {
		bool value;
		AOM_ask_value_logical(Tag, property_name.c_str(), &value);
		return value ? "true" : "false";
	}
	else if (valtype == PROP_short) {
		// Handle short type
	}
	else if (valtype == PROP_string) {
		char* value;
		AOM_ask_value_string(Tag, property_name.c_str(), &value);
		ECHO(("property_name: %s, value=%s\n", property_name.c_str(), value));
		if (property_name == "bl_sequence_no") {
			if (value[0] == '\0') {
				value = "10";
				ECHO(("BL_SEQUENCE_NO '' found, changing to 10...\n"));
			}
		}
		return value;
	}
	else if (valtype == PROP_typed_reference) {
		tag_t value_tag;
		char* value;
		AOM_ask_value_tag(Tag, property_name.c_str(), &value_tag);
		if (value_tag == 0) {
			return "ks";
		}
		else {
			AOM_ask_value_string(value_tag, "object_string", &value);
			return value;
		}
	}
	else if (valtype == PROP_untyped_reference) {
		tag_t value_tag;
		char* value;
		AOM_ask_value_tag(Tag, property_name.c_str(), &value_tag);
		if (value_tag == 0) {
			return "";
		}
		else if (property_name.compare("bl_parent") == 0)
		{
			AOM_ask_value_string(value_tag, "bl_item_item_id", &value);
			return value;
		}
		else {
			AOM_ask_value_string(value_tag, "object_string", &value);
			return value;
		}
	}
	// If the valtype is not handled, return an empty string
	return "";
}

std::vector<std::string> split(const std::string& s, char delim) {
	std::vector<std::string> result;
	std::stringstream ss(s);
	std::string item;

	while (getline(ss, item, delim)) {
		result.push_back(item);
	}

	return result;
}

std::string ExportPathFile(tag_t ItemRev, std::string Values, std::string Path, std::string ItemId) {

	std::vector<std::string> splittedValues = split(Values, ',');
	std::string AttachType = splittedValues[0];
	std::string Relation = splittedValues[1];

	std::string cesta;

	tag_t relationType;

	GRM_find_relation_type(Relation.c_str(), &relationType);

	int n_specs;
	tag_t* specs;
	char* type_name;
	char* dataset_name;
	GRM_list_secondary_objects_only(ItemRev, relationType, &n_specs, &specs);

	for (int i = 0; i < n_specs; i++) {

		tag_t type_tag;
		char* type_name;
		char* dataset_name;
		int reference_count = 0;
		TCTYPE_ask_object_type(specs[i], &type_tag);
		TCTYPE_ask_name2(type_tag, &type_name);
		AOM_ask_value_string(specs[i], "object_string", &dataset_name);

		std::string s = type_name;
		if (s == "PDF" && AttachType == "PDF") {
			std::string s = dataset_name;
			//cesta = Path + "PDF\\" + ItemId + s;
			AE_ask_dataset_ref_count(specs[i], &reference_count);
			if (reference_count > 0)
			{

				for (int t = 0; t < reference_count; t++)
				{
					tag_t referenced_object;
					char* reference_name;
					AE_reference_type_t ref_type;

					AE_find_dataset_named_ref2(specs[i], t, &reference_name, &ref_type, &referenced_object);
					cesta = ask_imanfile_path(referenced_object);
					ECHO(("cesta %s \n", cesta));
				}
			}
		}
		if (s == "DXF" && AttachType == "DXF") {
			std::string s = dataset_name;
			//cesta = Path + "DXF\\" + ItemId + s;
			AE_ask_dataset_ref_count(specs[i], &reference_count);
			if (reference_count > 0)
			{

				for (int t = 0; t < reference_count; t++)
				{
					tag_t referenced_object;
					char* reference_name,
						*file_path;
					AE_reference_type_t ref_type;

					AE_find_dataset_named_ref2(specs[i], t, &reference_name, &ref_type, &referenced_object);
					cesta = ask_imanfile_path(referenced_object);
					ECHO(("cesta %s \n", cesta));
				}
			}
		}
		MEM_free(dataset_name);
		MEM_free(type_name);
	}
	if (n_specs == 0) {
		return "";
	}
	else {
		return cesta;
	}

}

std::string ExportAttachments(tag_t ItemRev, std::string Values, std::string Path, std::string ItemId) {

	std::vector<std::string> splittedValues = split(Values, ',');
	std::string AttachType = splittedValues[0];
	std::string Relation = splittedValues[1];

	std::string cesta;

	tag_t relationType;

	GRM_find_relation_type(Relation.c_str(), &relationType);

	int n_specs;
	tag_t* specs;
	char* type_name;
	char* dataset_name;
	GRM_list_secondary_objects_only(ItemRev, relationType, &n_specs, &specs);

	for (int i = 0; i < n_specs; i++) {

		tag_t type_tag;
		char* type_name;
		char* dataset_name;
		TCTYPE_ask_object_type(specs[i], &type_tag);
		TCTYPE_ask_name2(type_tag, &type_name);
		AOM_ask_value_string(specs[i], "object_string", &dataset_name);

		std::string str = type_name;
		if (str == "PDF" && AttachType == "PDF") {
			std::string s = dataset_name;
			std::replace(s.begin(), s.end(), '/', '_');
			cesta = Path + "PDF\\" + s +".pdf";
			AE_export_named_ref(specs[i], "PDF_Reference", cesta.c_str());
		}
		if (str == "DXF" && AttachType == "DXF") {
			std::string s = dataset_name;
			std::replace(s.begin(), s.end(), '/', '_');
			cesta = Path + "DXF\\" + s + ".dxf";
			AE_export_named_ref(specs[i], "DXF", cesta.c_str());
		}
		MEM_free(dataset_name);
		MEM_free(type_name);
	}
	if (n_specs == 0) {
		return "";
	}
	else {
		return cesta;
	}
}

std::string findAttrFromRelation(tag_t child, char* Relation, std::string property_name) {
	int count = 0;
	tag_t relation_type;
	tag_t* primary_objects;
	tag_t primary_obj;
	std::string value;


	GRM_find_relation_type(Relation, &relation_type);

	GRM_list_primary_objects_only(child, relation_type, &count, &primary_objects);

	if (count == 1)
	{
		primary_obj = primary_objects[0];
		ECHO(("1 primary object found\n"));
	}
	else if (count > 1)
	{
		ECHO(("%d primary objects found\n", count));
		for (int i = 0; i < count; i++)
		{
			char* Type;
			WSOM_ask_object_type2(primary_objects[i], &Type);//Returns the object type of the specified WorkspaceObject.
			if (strcmp(Type, "TPV4_nak_polRevision") == 0)
			{
				primary_obj = primary_objects[i];
			}
		}
	}
	
	if (primary_objects) {
		MEM_free(primary_objects);
	}

	value = readElement(primary_obj, property_name);
	ECHO(("Value: %s\n", value.c_str()));

	return value;
}

vectorArray RecursiveBOM(const std::string& InputAttrValues, const std::string& RevId, vectorArray csv, vectorArray config, bool addParent, std::string AttachPath)
{
	int n_children;
	tag_t
		window,
		tBOMTopLine,
		*children;
	ECHO(("L:%d \n", __LINE__));
	std::vector<tag_t> ItemAndRev;
	ItemAndRev = find_itemRevision(InputAttrValues, RevId);
	tag_t Item = ItemAndRev[0];
	tag_t ItemRev = ItemAndRev[1];

	BOM_create_window(&window);
	BOM_set_window_top_line(window, NULLTAG, ItemRev, NULLTAG, &tBOMTopLine);

	BOM_line_ask_child_lines(tBOMTopLine, &n_children, &children);

	if (addParent) {
		const std::string ParentId = InputAttrValues;
		char
			* ItemId,
			*RevId;
		tag_t Tag = tBOMTopLine;
		std::vector<std::string> csv_row;

		AOM_ask_value_string(tBOMTopLine, "bl_item_item_id", &ItemId);
		AOM_ask_value_string(tBOMTopLine, "bl_rev_item_revision_id", &RevId);

		ItemAndRev = find_itemRevision(ItemId, RevId);
		Item = ItemAndRev[0];
		ItemRev = ItemAndRev[1];
		ECHO(("L:%d \n", __LINE__));
		for (int j = 0; j < config.size(); j++) {
			std::string value;
			if (config[j][0] == "BOM") {
				value = readElement(tBOMTopLine, config[j][1]);
			}
			if (config[j][0] == "Item") {
				value = readElement(Item, config[j][1]);
			}
			if (config[j][0] == "Rev") {
				value = readElement(ItemRev, config[j][1]);
			}
			if (config[j][0] == "Rev4Relation") {
				std::vector<std::string> splittedValues = split(config[j][1], ',');
				std::string Attr = splittedValues[0];
				std::string Relation = splittedValues[1];
				ECHO(("L:%d \n", __LINE__));
				tag_t* primary_obj;
					int n = CountInRelation(ItemRev, Relation, &primary_obj);
					ECHO(("L:%d count relation %d \n", __LINE__, n));
				value = readElement(primary_obj[0], Attr);
			}
			if (config[j][0] == "GetImanFilePath") {
				value = ExportPathFile(ItemRev, config[j][1], AttachPath, ItemId);
			}

			if (config[j][0] == "Export") {
				value = ExportAttachments(ItemRev, config[j][1], AttachPath, ItemId);
			}
			if (config[j][0] == "Writte") {
				value = config[j][1];
			}
			if (config[j][0] == "Relation") {
				std::vector<std::string> parameters = split(config[j][1], ',');
				std::string relation = parameters[0];
				std::string property_name = parameters[1];
				char* item_type;
				//ITEM_ask_type2(ItemRev, &item_type);
				AOM_ask_value_string(ItemRev, "object_type", &item_type);

				ECHO(("Item Type: %s\n", item_type));

				if (strcmp(item_type, "TPV4_nak_dilRevision") == 0) {
					value = findAttrFromRelation(ItemRev, (char*)relation.c_str(), property_name);
				}
				else {
					value = "";
				}
			}

			//všechny ; symboly jsou nahrazeny , aby se nepoškodil formát csv
			std::replace(value.begin(), value.end(), ';', ',');

			csv_row.push_back(value);
		}

		// push new row into the csv
		csv.push_back(csv_row);

		MEM_free(ItemId);
		MEM_free(RevId);
	}

	// Iterate over chlidren (if there are any) and call recursiveBOM function on them
	for (int i = 0; i < n_children; i++)
	{
		const std::string ParentId = InputAttrValues;
		char
			* ItemId,
			*RevId;
		tag_t Tag = children[i];
		std::vector<std::string> csv_row;


		AOM_ask_value_string(children[i], "bl_item_item_id", &ItemId);
		AOM_ask_value_string(children[i], "bl_rev_item_revision_id", &RevId);

		ItemAndRev = find_itemRevision(ItemId, RevId);
		Item = ItemAndRev[0];
		ItemRev = ItemAndRev[1];

		for (int j = 0; j < config.size(); j++) {
			std::string value;
			if (config[j][0] == "BOM") {
				value = readElement(children[i], config[j][1]);
			}
			if (config[j][0] == "Item") {
				value = readElement(Item, config[j][1]);
			}
			if (config[j][0] == "Rev") {
				value = readElement(ItemRev, config[j][1]);
			}
			if (config[j][0] == "Rev4Relation") {
				std::vector<std::string> splittedValues = split(config[j][1], ',');
				std::string Attr = splittedValues[0];
				std::string Relation = splittedValues[1];
				ECHO(("L:%d \n", __LINE__));
				tag_t* primary_obj;
				int n = CountInRelation(ItemRev, Relation, &primary_obj);
				ECHO(("L:%d count relation %d \n", __LINE__,n));
				value = readElement(primary_obj[0], Attr);
			}
			if (config[j][0] == "Export") {
				value = ExportAttachments(ItemRev, config[j][1], AttachPath, ItemId);
			}
			if (config[j][0] == "GetImanFilePath") {
				value = ExportPathFile(ItemRev, config[j][1], AttachPath, ItemId);
			}
			if(config[j][0] == "Writte") {
				value = config[j][1];
			}
			if (config[j][0] == "Relation") {
				std::vector<std::string> parameters = split(config[j][1], ',');
				std::string relation = parameters[0];
				std::string property_name = parameters[1];
				char* item_type;
				//ITEM_ask_type2(ItemRev, &item_type);
				AOM_ask_value_string(ItemRev, "object_type", &item_type);

				ECHO(("Item Type: %s\n", item_type));

				if (strcmp(item_type, "TPV4_nak_dilRevision") == 0) {
					value = findAttrFromRelation(ItemRev, (char*)relation.c_str(), property_name);
				}
				else {
					value = "";
				}
			}

			//všechny ; symboly jsou nahrazeny , aby se nepoškodil formát csv
			std::replace(value.begin(), value.end(), ';', ',');

			csv_row.push_back(value);
		}

		// push new row into the csv
		csv.push_back(csv_row);

		csv = RecursiveBOM(ItemId, RevId, csv, config, false, AttachPath);

		MEM_free(ItemId);
		MEM_free(RevId);
	}

	MEM_free(children);
	BOM_save_window(window);
	BOM_close_window(window);

	return csv;
}
void CallBridge(std::string file)
{
	char help[50];
	strcpy(help, file.c_str());
	ECHO(("file %s \n", help));
	//char ImportTPV[256]="C:\\TC4TPV\\TCCom\\TCCom.jar \"";

	char ImportTPV[256] = "call C:\\SPLM\\Apps\\Export\\";
	ECHO(("%s \n", callBat));
	if (strcmp(callBat, "0") == 0)
		strcat(ImportTPV, "TC2ERP.bat");
	else
		strcat(ImportTPV, callBat);
	//strcat(ImportTPV, " \"C:\\SPLM\\Apps\\Export\\csv\\");
	strcat(ImportTPV, " ");
	strcat(ImportTPV, help);
	//strcat(ImportTPV, ".csv");
	//strcat(ImportTPV, "\"");
	//ECHO(("%s \n",ImportTPV);
	//int error =system(ImportTPV);
	//ECHO(("L: %d error code %d \n", __LINE__, error));
	exec(ImportTPV);
	//	system("call C:\\TC4TPV\\TCCom\\run.bat");
	ECHO(("%s\n", ImportTPV));
	//system("call TC2TPV.bat");
}

int createCSV(vectorArray csv, std::string exportFolderPath, vectorArray config, char* ItemId, char* RevId,std::string FirstLine)
{
	std::ofstream file;
	std::string filePath = exportFolderPath + "\\export" + ItemId + "-" + RevId + ".csv";
	ECHO(("L:%d filePath %s \n", __LINE__, filePath.c_str()));
	file.open(filePath);
	if(definefirstline)
	{ 
		ECHO(("L:%d First Line %s \n", __LINE__, FirstLine.c_str()));
		file << FirstLine;
	}
	else
	{
		for (int j = 0; j < config.size(); j++)
		{
		file << config[j][1] << "#";
		}
	}
	for (int i = 0; i < csv.size(); i++) {
		file << "\n";
		if (config[0][2].compare("varchar") == 0) file << "'";
		if (config[0][2].compare("quantity") == 0 && csv[i][0].size()==0) file << "1";
		file << csv[i][0];
		if (config[0][2].compare("varchar") == 0) file << "'";
		for (int j = 1; j < csv[0].size(); j++) {
			
			file << "#";
			if (config[j][2].compare("varchar")==0) file << "'";
			if (config[j][2].compare("quantity") == 0 && csv[i][j].size() == 0) file << "1";
			file << csv[i][j];
			if (config[j][2].compare("varchar")==0) file << "'";
		}
	}
	file.close();
	Sleep(5000);
	CallBridge(filePath);
	return 0;
}

vectorArray load_config(std::string configPath)
{
	vectorArray config;

	// Open the file
	std::ifstream file(configPath);

	if (file.is_open()) {
		std::string line;

		// Read each line of the file
		while (std::getline(file, line)) {
			std::istringstream iss(line);
			std::string key, value,exportType;
			/*if (std::getline(iss, key, '#'))
			{
				key.erase(0, key.find_first_not_of(" \t\r\n"));
					key.erase(key.find_last_not_of(" \t\r\n") + 1);
					ECHO(("L:%d key %s \n", __LINE__, key.c_str()));
					
			}*/
			if (std::getline(iss, key, '#') && std::getline(iss, value, '#') && std::getline(iss, exportType)) {
				key.erase(0, key.find_first_not_of(" \t\r\n"));
				key.erase(key.find_last_not_of(" \t\r\n") + 1);
				ECHO(("L:%d key %s \n", __LINE__, key.c_str()));
				value.erase(0, value.find_first_not_of(" \t\r\n"));
				value.erase(value.find_last_not_of(" \t\r\n") + 1);
				ECHO(("L:%d key %s \n", __LINE__, value.c_str()));
				exportType.erase(0, exportType.find_first_not_of(" \t\r\n"));
				exportType.erase(exportType.find_last_not_of(" \t\r\n") + 1);
				ECHO(("L:%d value %s \n", __LINE__, exportType.c_str()));
				config.push_back({key, value, exportType });
			}
		}
		ECHO(("Config loaded succesfully.\n"));
		file.close();
	}
	else {
		ECHO(("Config failed to load.\n"));
	}

	return config;
}


std::vector<std::string> read_properties(EPM_action_message_t msg)
{
	char
		* Argument,
		*Flag,
		*Value;
	std::string
		AttachPath,
		config,
		FirstLine;
	std::vector<std::string> props;

	int ArgumentCount = TC_number_of_arguments(msg.arguments);

	while (ArgumentCount-- > 0)
	{
		Argument = TC_next_argument(msg.arguments);
		ITK_ask_argument_named_value(Argument, &Flag, &Value);
		if (strcmp("ConfigPath", Flag) == 0 && Value != nullptr)
		{
			config = Value;
		}
		if (strcmp("ExportFolderPath", Flag) == 0 && Value != nullptr)
		{
			AttachPath = Value;
		}
		if(strcmp("FirstLine", Flag) == 0 && Value != nullptr)
		{
			definefirstline = true;
			FirstLine= Value;
			ECHO(("L:%d first line %s \n",__LINE__, Value));
			ECHO(("L:%d first line %s \n", __LINE__, FirstLine.c_str()));
		}if (strcmp("callBat", Flag) == 0)
		{
			ECHO(("callBat is %s \n", Value));
			strcpy(callBat, Value);
		}

		MEM_free(Flag);
		MEM_free(Value);
	}
	props.push_back(config);
	props.push_back(AttachPath);
	props.push_back(FirstLine);

	return props;
}

int TPV_BOM2CSV_TC14(EPM_action_message_t msg)
{
	ECHO(("*******************ZAÈATEK BOM2CSV**********************\n"));
	vectorArray
		csv,
		config;
	std::string AttachPath,
		FirstLine;

	int n_attachments;
	char
		* class_name,
		*ItemId,
		*RevId;
	tag_t
		RootTask,
		*attachments,
		class_tag;
	definefirstline = false;

	strcpy(callBat, "0");
	std::vector<std::string> props = read_properties(msg);
	ECHO(("Pøed configem\n"));
	//return 0;
	config = load_config(props[0]);
	ECHO(("Po configu\n"));
	AttachPath = props[1];
	ECHO(("L:%d First Line %s \n", __LINE__, props[2].c_str()));
	FirstLine=props[2];
	ECHO(("L:%d First Line %s \n", __LINE__, FirstLine.c_str()));
	int config0size = config[0].size();
	int configsize = config.size();
	std::string exportFolderPath = config[0][1];
	ECHO(("L:%d  \n", __LINE__));
	std::string itemType = config[1][1];
	ECHO(("L:%d  \n", __LINE__));
	config.erase(config.begin(), config.begin() + 2);
	ECHO(("L:%d  \n", __LINE__));
	EPM_ask_root_task(msg.task, &RootTask);
	EPM_ask_attachments(RootTask, EPM_target_attachment, &n_attachments, &attachments);
	for (int i = 0; i < n_attachments; i++)
	{

		POM_class_of_instance(attachments[i], &class_tag);

		// Returns the name of the given class
		POM_name_of_class(class_tag, &class_name);

		const char* c = itemType.c_str();
		if (strcmp(class_name, c) == 0)
		{
			AOM_ask_value_string(attachments[i], "item_id", &ItemId);
			AOM_ask_value_string(attachments[i], "current_revision_id", &RevId);
			ECHO(("L:%d  \n", __LINE__));
			csv = RecursiveBOM(ItemId, RevId, csv, config, true, AttachPath);
			ECHO(("L:%d  \n", __LINE__));
			createCSV(csv, exportFolderPath, config, ItemId, RevId, FirstLine);
			
		}
	}
	return 0;
}