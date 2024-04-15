#include "deck_manager.h"
#include "data_manager.h"
#include "network.h"
#include "game.h"

namespace ygo {

#ifndef YGOPRO_SERVER_MODE
char DeckManager::deckBuffer[0x10000];
#endif
DeckManager deckManager;

void DeckManager::LoadLFListSingle(const char* path) {
	LFList* cur = nullptr;
	FILE* fp = fopen(path, "r");
	char linebuf[256];
	wchar_t strBuffer[256];
	if(fp) {
		while(fgets(linebuf, 256, fp)) {
			if(linebuf[0] == '#')
				continue;
			if(linebuf[0] == '!') {
				int sa = BufferIO::DecodeUTF8(&linebuf[1], strBuffer);
				while(strBuffer[sa - 1] == L'\r' || strBuffer[sa - 1] == L'\n' ) sa--;
				strBuffer[sa] = 0;
				LFList newlist;
				_lfList.push_back(newlist);
				cur = &_lfList[_lfList.size() - 1];
				cur->listName = strBuffer;
				cur->hash = 0x7dfcee6a;
				continue;
			}
			int p = 0;
			while(linebuf[p] != ' ' && linebuf[p] != '\t' && linebuf[p] != 0) p++;
			if(linebuf[p] == 0)
				continue;
			linebuf[p++] = 0;
			int sa = p;
			int code = atoi(linebuf);
			if(code == 0)
				continue;
			while(linebuf[p] == ' ' || linebuf[p] == '\t') p++;
			while(linebuf[p] != ' ' && linebuf[p] != '\t' && linebuf[p] != 0) p++;
			linebuf[p] = 0;
			int count = atoi(&linebuf[sa]);
			if(!cur) continue;
			cur->content[code] = count;
			cur->hash = cur->hash ^ ((code << 18) | (code >> 14)) ^ ((code << (27 + count)) | (code >> (5 - count)));
		}
		fclose(fp);
	}
}
void DeckManager::LoadLFList() {
#ifdef SERVER_PRO2_SUPPORT
	LoadLFListSingle("config/lflist.conf");
#endif
	LoadLFListSingle("expansions/lflist.conf");
	LoadLFListSingle("lflist.conf");
	LFList nolimit;
	nolimit.listName = L"N/A";
	nolimit.hash = 0;
	_lfList.push_back(nolimit);
}
const wchar_t* DeckManager::GetLFListName(int lfhash) {
	auto lit = std::find_if(_lfList.begin(), _lfList.end(), [lfhash](const ygo::LFList& list) {
		return list.hash == lfhash;
	});
	if(lit != _lfList.end())
		return lit->listName.c_str();
	return dataManager.unknown_string;
}
const std::unordered_map<int, int>* DeckManager::GetLFListContent(int lfhash) {
	auto lit = std::find_if(_lfList.begin(), _lfList.end(), [lfhash](const ygo::LFList& list) {
		return list.hash == lfhash;
	});
	if(lit != _lfList.end())
		return &lit->content;
	return nullptr;
}
static int checkAvail(int ot, int avail) {
	if((ot & avail) == avail)
		return 0;
	if((ot & AVAIL_OCG) && !(avail == AVAIL_OCG))
		return DECKERROR_OCGONLY;
	if((ot & AVAIL_TCG) && !(avail == AVAIL_TCG))
		return DECKERROR_TCGONLY;
	return DECKERROR_NOTAVAIL;
}
int DeckManager::CheckDeck(Deck& deck, int lfhash, int rule) {
	std::unordered_map<int, int> ccount;
	auto list = GetLFListContent(lfhash);
	if(!list)
		return 0;
	int dc = 0;
	if(deck.main.size() != 50)
		return (DECKERROR_MAINCOUNT << 28) + deck.main.size();
	const int rule_map[6] = { AVAIL_OCG, AVAIL_TCG, AVAIL_SC, AVAIL_CUSTOM, AVAIL_OCGTCG, 0 };
	int avail = rule_map[rule];
	for(size_t i = 0; i < deck.main.size(); ++i) {
		code_pointer cit = deck.main[i];
		int gameruleDeckError = checkAvail(cit->second.ot, avail);
		if(gameruleDeckError)
			return (gameruleDeckError << 28) + cit->first;
		if(cit->second.type & (TYPE_FUSION | TYPE_SYNCHRO | TYPE_XYZ | TYPE_TOKEN | TYPE_LINK))
			return (DECKERROR_EXTRACOUNT << 28);
		int code = cit->second.alias ? cit->second.alias : cit->first;
		ccount[code]++;
		dc = ccount[code];
		if(dc > 4)
			return (DECKERROR_CARDCOUNT << 28) + cit->first;
		auto it = list->find(code);
		if(it != list->end() && dc > it->second)
			return (DECKERROR_LFLIST << 28) + cit->first;
	}
	for(size_t i = 0; i < deck.extra.size(); ++i) {
		code_pointer cit = deck.extra[i];
		int gameruleDeckError = checkAvail(cit->second.ot, avail);
		if(gameruleDeckError)
			return (gameruleDeckError << 28) + cit->first;
		int code = cit->second.alias ? cit->second.alias : cit->first;
		ccount[code]++;
		dc = ccount[code];
		if(dc > 4)
			return (DECKERROR_CARDCOUNT << 28) + cit->first;
		auto it = list->find(code);
		if(it != list->end() && dc > it->second)
			return (DECKERROR_LFLIST << 28) + cit->first;
	}
	// for(size_t i = 0; i < deck.side.size(); ++i) {
	// 	code_pointer cit = deck.side[i];
	// 	int gameruleDeckError = checkAvail(cit->second.ot, avail);
	// 	if(gameruleDeckError)
	// 		return (gameruleDeckError << 28) + cit->first;
	// 	int code = cit->second.alias ? cit->second.alias : cit->first;
	// 	ccount[code]++;
	// 	dc = ccount[code];
	// 	if(dc > 4)
	// 		return (DECKERROR_CARDCOUNT << 28) + cit->first;
	// 	auto it = list->find(code);
	// 	if(it != list->end() && dc > it->second)
	// 		return (DECKERROR_LFLIST << 28) + cit->first;
	// }
	return 0;
}
int DeckManager::LoadDeck(Deck& deck, int* dbuf, int mainc,int extrac, int sidec, bool is_packlist, bool forduel) {
	deck.clear();
	int code;
	int errorcode = 0;
	CardDataC cd;
	for(int i = 0; i < mainc; ++i) {
		code = dbuf[i];
		if(!dataManager.GetData(code, &cd)) {
			errorcode = code;
			continue;
		}
		if(cd.type & TYPE_TOKEN)
			continue;
		else if(is_packlist) {
			deck.main.push_back(dataManager.GetCodePointer(code));
			continue;
		}
		else if(deck.main.size() < 51) {
			if(CheckCard(deck,cd))
				deck.main.push_back(dataManager.GetCodePointer(code));
		}
	}
	for(int i = 0; i < extrac; ++i) {
		code = dbuf[mainc + i];
		if(!dataManager.GetData(code, &cd)) {
			errorcode = code;
			continue;
		}
		if(cd.type & TYPE_TOKEN)
			continue;
		else if(deck.extra.size() < 30){
			if(CheckCard(deck,cd))
				deck.extra.push_back(dataManager.GetCodePointer(code));
		}
	}
	if(deck.Gcheck.size() == 4 && forduel){
		for(int i = 0; i < sidec; ++i) {
			code = dbuf[mainc +extrac + i];
			if(!dataManager.GetData(code, &cd)) {
				errorcode = code;
				continue;
			}
			if(cd.type & TYPE_TOKEN)
				continue;
			if(deck.extra.size() < 15)
				if(CheckCard(deck,cd))
					deck.extra.push_back(dataManager.GetCodePointer(code));
		}
	}
	else if (!forduel)
	{
		for(int i = 0; i < sidec; ++i) {
			code = dbuf[mainc +extrac + i];
			if(!dataManager.GetData(code, &cd)) {
				errorcode = code;
				continue;
			}
			if(cd.type & TYPE_TOKEN)
				continue;
			if(deck.side.size() < 15)
				if(CheckCard(deck,cd))
					deck.side.push_back(dataManager.GetCodePointer(code));
		}
	}
	
	return errorcode;
}
bool DeckManager::CheckCard(Deck& deck, CardDataC cd)
{
	std::vector<code_pointer> deck_all = deck.main;
	deck_all.insert(deck_all.begin(),deck.extra.begin(),deck.extra.end());
	deck_all.push_back(dataManager.GetCodePointer(cd.code));

	uint16 deckcountry = 0;
	int trigger_card = 0;
	int trigger_heal = 0;
	int trigger_crit = 0;
	int trigger_draw = 0;
	int trigger_front = 0;
	int protector =0;
	bool trigger_over = false;
	bool regalis_piece = false;

	for(auto& pcard : deck_all){

		CardDataC cd = pcard->second;

		int country = cd.country & 0x0FFF;
		if (deckcountry == 0 && (!(country & 0x1)) && (country != 0 && (country & (country - 1)) == 0))
		{
			deckcountry = country;
		}

		//势力
		if (deckcountry != 0 || cd.country & 0x1)
		{
			if (!(cd.country & deckcountry) && !(cd.country & 0x1))
			{
				return false;
			}
		}

		//触发
		if (!(cd.race & RACE_WARRIOR))
		{
			if (trigger_card >= 16)
			{
				return false;
			}
			if(cd.race & RACE_SPELLCASTER){
				if (trigger_crit >= 8)
				{
					return false;
				}
				else{
					trigger_crit++;
					trigger_card++;
				}
			}
			if(cd.race & RACE_FAIRY){
				if (trigger_draw >= 8)
				{
					return false;
				}
				else{
					trigger_draw++;
					trigger_card++;
				}
			}
			if(cd.race & RACE_FIEND){
				if (trigger_heal >= 4)
				{
					return false;
				}
				else{
					trigger_heal++;
					trigger_card++;
				}
			}	
			if(cd.race & RACE_ZOMBIE){
				if (trigger_front >= 8)
				{
					return false;
				}
				else{
					trigger_front++;
					trigger_card++;
				}
			}	
			if(cd.race & RACE_MACHINE){
				if (trigger_over)
				{
					return false;
				}
				else{
					trigger_over = true;
					trigger_card++;
				}
			}			
		}
		
		//守护者
		if (cd.category & 0x1){
			if(protector>3){
				return false;
			}
			else
			{
				protector++;
			}
			
		}

		//结晶碎片
		if (cd.is_setcode(0x204))
		{
			if (regalis_piece)
			{
				return false;
			}
			else
			{
				regalis_piece = true;
			}
		}
	}

	return true;
}
bool DeckManager::CheckCardEx(Deck& deck, CardDataC cd)
{
	if(cd.level>4){
		return false;
	}
	if(cd.type & TYPE_SPELL && (!cd.is_setcode(0xc042) && cd.code != 10602015 && cd.code != 10707035)){
		return false;
	}
	int monster_marble_chk = 0;
	int monster_marble_dragon_chk = 0;
	int disaster_chk = 0;
	
	bool monster_marble = false;
	bool monster_marble_dragon = false;
	bool disaster = false;

	for(auto& pcard : deck.extra){
		//检查rider等级
		if(pcard->second.level == cd.level){
			return false;
		}
		if(pcard->second.country == 0x200){
			if (pcard->second.code == 10602015)
				monster_marble_dragon_chk++;
			monster_marble_chk++;
		}
		else if (pcard->second.code == 10409097 || pcard->second.is_setcode(0xc042)){
			disaster_chk++;
		}
		//g卡组检测
		if ((cd.code == 10910004 || cd.code == 10910003 || cd.code == 10910002 || cd.code == 10910001))
		{
			auto it = std::find(deck.Gcheck.begin(), deck.Gcheck.end(),cd.code);
				if(it == deck.Gcheck.end())
					deck.Gcheck.push_back(cd.code);
		}
		else if ((cd.code == 10909004 || cd.code == 10909003 || cd.code == 10909002 || cd.code == 10909001))
		{
			auto it = std::find(deck.Gcheck.begin(), deck.Gcheck.end(),cd.code);
				if(it == deck.Gcheck.end())
					deck.Gcheck.push_back(cd.code);
		}
		else if ((cd.code == 10904001 || cd.code == 10904002 || cd.code == 10904003 || cd.code == 10904004))
		{
			auto it = std::find(deck.Gcheck.begin(), deck.Gcheck.end(),cd.code);
				if(it == deck.Gcheck.end())
					deck.Gcheck.push_back(cd.code);
		}
		else if ((cd.code == 10903004 || cd.code == 10903002 || cd.code == 10903003 || cd.code == 10903001))
		{
			auto it = std::find(deck.Gcheck.begin(), deck.Gcheck.end(),cd.code);
				if(it == deck.Gcheck.end())
					deck.Gcheck.push_back(cd.code);
		}
	}
	if (monster_marble_chk != 0){
		monster_marble = true;
		monster_marble_chk = 0;
	}
	else
		monster_marble = false;

	if (monster_marble_dragon_chk != 0){
		monster_marble_dragon = true;
		monster_marble_dragon_chk = 0;
	}
	else
		monster_marble_dragon = false;

	if (disaster_chk != 0){
		disaster = true;
		disaster_chk = 0;
	}
	else
		disaster = false;
	
	//龙树
	if (disaster){
		if (cd.code == 10409097 || cd.is_setcode(0xc042))
		{
			return true;
		}
		else if (cd.race & RACE_MACHINE)
			return true;
		return false;
	}

	//怪物弹珠
	if (monster_marble){
		if (cd.country == 0x200)
		{
			if (cd.code == 10602015 && monster_marble_dragon)
				return false;
			return true;
		}
		return false;
	}

	return true;
}
bool DeckManager::LoadSide(Deck& deck, int* dbuf, int mainc, int sidec) {
	std::unordered_map<int, int> pcount;
	std::unordered_map<int, int> ncount;
	for(size_t i = 0; i < deck.main.size(); ++i)
		pcount[deck.main[i]->first]++;
	for(size_t i = 0; i < deck.extra.size(); ++i)
		pcount[deck.extra[i]->first]++;
	for(size_t i = 0; i < deck.side.size(); ++i)
		pcount[deck.side[i]->first]++;
	Deck ndeck;
	//LoadDeck(ndeck, dbuf, mainc, sidec);
	if(ndeck.main.size() != deck.main.size() || ndeck.extra.size() != deck.extra.size())
		return false;
	for(size_t i = 0; i < ndeck.main.size(); ++i)
		ncount[ndeck.main[i]->first]++;
	for(size_t i = 0; i < ndeck.extra.size(); ++i)
		ncount[ndeck.extra[i]->first]++;
	for(size_t i = 0; i < ndeck.side.size(); ++i)
		ncount[ndeck.side[i]->first]++;
	for(auto cdit = ncount.begin(); cdit != ncount.end(); ++cdit)
		if(cdit->second != pcount[cdit->first])
			return false;
	deck = ndeck;
	return true;
}
#ifndef YGOPRO_SERVER_MODE
void DeckManager::GetCategoryPath(wchar_t* ret, int index, const wchar_t* text) {
	wchar_t catepath[256];
	switch(index) {
	case 0:
		myswprintf(catepath, L"./pack");
		break;
	case 1:
		myswprintf(catepath, mainGame->gameConf.bot_deck_path);
		break;
	case -1:
	case 2:
	case 3:
		myswprintf(catepath, L"./deck");
		break;
	default:
		myswprintf(catepath, L"./deck/%ls", text);
	}
	BufferIO::CopyWStr(catepath, ret, 256);
}
void DeckManager::GetDeckFile(wchar_t* ret, irr::gui::IGUIComboBox* cbCategory, irr::gui::IGUIComboBox* cbDeck) {
	wchar_t filepath[256];
	wchar_t catepath[256];
	wchar_t* deckname = (wchar_t*)cbDeck->getItem(cbDeck->getSelected());
	if(deckname != NULL) {
		GetCategoryPath(catepath, cbCategory->getSelected(), cbCategory->getText());
		myswprintf(filepath, L"%ls/%ls.ydk", catepath, deckname);
		BufferIO::CopyWStr(filepath, ret, 256);
	}
	else {
		BufferIO::CopyWStr(L"", ret, 256);
	}
}
bool DeckManager::LoadDeck(irr::gui::IGUIComboBox* cbCategory, irr::gui::IGUIComboBox* cbDeck) {
	wchar_t filepath[256];
	GetDeckFile(filepath, cbCategory, cbDeck);
	bool is_packlist = cbCategory->getSelected() == 0;
	bool res = LoadDeck(filepath, is_packlist);
	if(res && mainGame->is_building)
		mainGame->deckBuilder.RefreshPackListScroll();
	return res;
}
FILE* DeckManager::OpenDeckFile(const wchar_t* file, const char* mode) {
#ifdef WIN32
	FILE* fp = _wfopen(file, (wchar_t*)mode);
#else
	char file2[256];
	BufferIO::EncodeUTF8(file, file2);
	FILE* fp = fopen(file2, mode);
#endif
	return fp;
}
IReadFile* DeckManager::OpenDeckReader(const wchar_t* file) {
#ifdef WIN32
	IReadFile* reader = dataManager.FileSystem->createAndOpenFile(file);
#else
	char file2[256];
	BufferIO::EncodeUTF8(file, file2);
	IReadFile* reader = dataManager.FileSystem->createAndOpenFile(file2);
#endif
	return reader;
}
bool DeckManager::LoadDeck(const wchar_t* file, bool is_packlist) {
	IReadFile* reader = OpenDeckReader(file);
	if(!reader) {
		wchar_t localfile[64];
		myswprintf(localfile, L"./deck/%ls.ydk", file);
		reader = OpenDeckReader(localfile);
	}
	if(!reader && !mywcsncasecmp(file, L"./pack", 6)) {
		wchar_t zipfile[64];
		myswprintf(zipfile, L"%ls", file + 2);
		reader = OpenDeckReader(zipfile);
	}
	if(!reader)
		return false;
	size_t size = reader->getSize();
	if(size >= 0x20000) {
		reader->drop();
		return false;
	}
	memset(deckBuffer, 0, sizeof(deckBuffer));
	reader->read(deckBuffer, size);
	reader->drop();
	std::istringstream deckStream(deckBuffer);
	return LoadDeck(&deckStream, is_packlist);
}
bool DeckManager::LoadDeck(std::istringstream* deckStream, bool is_packlist) {
	int sp = 0, ct = 0, mainc = 0, extrac = 0, sidec = 0, code;
	int cardlist[300];
	bool is_side = false;
	bool is_extra = false;
	std::string linebuf;
	while(std::getline(*deckStream, linebuf) && ct < 300) {
		if(linebuf.find("#extra") != std::string::npos){
			is_extra = true;
			continue;
		}
		if(linebuf[0] == '!') {
			is_extra = false;
			is_side = true;
			continue;
		}
		if(linebuf[0] < '0' || linebuf[0] > '9')
			continue;
		sp = 0;
		while(linebuf[sp] >= '0' && linebuf[sp] <= '9') sp++;
		linebuf[sp] = 0;
		code = std::stoi(linebuf);
		cardlist[ct++] = code;
		if(is_extra){
			extrac++;
		}
		else if(is_side){
			sidec++;
		}
		else
		{
			mainc++;
		}
		
		// if(is_side) sidec++;
		// else mainc++;
	}
	LoadDeck(current_deck, cardlist, mainc, extrac, sidec, is_packlist);
	return true; // the above LoadDeck has return value but we ignore it here for now
}
bool DeckManager::SaveDeck(Deck& deck, const wchar_t* file) {
	if(!FileSystem::IsDirExists(L"./deck") && !FileSystem::MakeDir(L"./deck"))
		return false;
	FILE* fp = OpenDeckFile(file, "w");
	if(!fp)
		return false;
	fprintf(fp, "#created by ...\n#main\n");
	for(size_t i = 0; i < deck.main.size(); ++i)
		fprintf(fp, "%d\n", deck.main[i]->first);
	fprintf(fp, "#extra\n");
	for(size_t i = 0; i < deck.extra.size(); ++i)
		fprintf(fp, "%d\n", deck.extra[i]->first);
	fprintf(fp, "!side\n");
	for(size_t i = 0; i < deck.side.size(); ++i)
		fprintf(fp, "%d\n", deck.side[i]->first);
	fclose(fp);
	return true;
}
bool DeckManager::DeleteDeck(const wchar_t* file) {
#ifdef WIN32
	BOOL result = DeleteFileW(file);
	return !!result;
#else
	char filefn[256];
	BufferIO::EncodeUTF8(file, filefn);
	int result = unlink(filefn);
	return result == 0;
#endif
}
bool DeckManager::CreateCategory(const wchar_t* name) {
	if(!FileSystem::IsDirExists(L"./deck") && !FileSystem::MakeDir(L"./deck"))
		return false;
	if(name[0] == 0)
		return false;
	wchar_t localname[256];
	myswprintf(localname, L"./deck/%ls", name);
	return FileSystem::MakeDir(localname);
}
bool DeckManager::RenameCategory(const wchar_t* oldname, const wchar_t* newname) {
	if(!FileSystem::IsDirExists(L"./deck") && !FileSystem::MakeDir(L"./deck"))
		return false;
	if(newname[0] == 0)
		return false;
	wchar_t oldlocalname[256];
	wchar_t newlocalname[256];
	myswprintf(oldlocalname, L"./deck/%ls", oldname);
	myswprintf(newlocalname, L"./deck/%ls", newname);
	return FileSystem::Rename(oldlocalname, newlocalname);
}
bool DeckManager::DeleteCategory(const wchar_t* name) {
	wchar_t localname[256];
	myswprintf(localname, L"./deck/%ls", name);
	if(!FileSystem::IsDirExists(localname))
		return false;
	return FileSystem::DeleteDir(localname);
}
#endif //YGOPRO_SERVER_MODE
}
