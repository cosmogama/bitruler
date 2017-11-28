/**
* BitRuler measures file system usage to help you determine where the most space is being used on your disk
*/
#include "stdafx.h"

#define DEBUG 0

class FileSizeCategory {
public:

	FileSizeCategory(wstring name, LARGE_INTEGER maxSize) : name(name), maxSize(maxSize) { count.QuadPart = 0; }

	wstring name;
	LARGE_INTEGER maxSize;
	LARGE_INTEGER count;
};

typedef struct Node {
	LARGE_INTEGER size;
	wstring name;
	bool isDir;
	multimap<LONGLONG, Node*, greater< LONGLONG > > files;
	Node* parent = NULL;

	Node(wstring name, Node* parent) : name(name), parent(parent) { size.QuadPart = 0; }

	~Node() {
		for (pair<LONGLONG, Node*> p : files) {
			delete p.second;
		}
	}

	wstring getPath() {
		if (parent != NULL) {
			return parent->getPath() + L"\\" + name;
		}
		return name;
	}
} Node;

typedef struct Tree {
	Node* root;
	LARGE_INTEGER numFiles;
	LARGE_INTEGER numDirs;
	list<FileSizeCategory> fileSizeCategories;

	Tree(Node* root) : root(root) {
		numFiles.QuadPart = 0;
		numDirs.QuadPart = 0;

		{ LARGE_INTEGER maxSize;
		maxSize.QuadPart = 0;
		FileSizeCategory catEmpty(wstring(L"EMPTY"), maxSize);
		fileSizeCategories.push_back(catEmpty); }

		{ LARGE_INTEGER maxSize;
		maxSize.QuadPart = 1000; // 1KB
		FileSizeCategory catEmpty(wstring(L"0 - 1KB"), maxSize);
		fileSizeCategories.push_back(catEmpty); }

		{ LARGE_INTEGER maxSize;
		maxSize.QuadPart = 1000000; // 1MB
		FileSizeCategory catEmpty(wstring(L"1KB - 1MB"), maxSize);
		fileSizeCategories.push_back(catEmpty); }

		{ LARGE_INTEGER maxSize;
		maxSize.QuadPart = 50000000; // 50MB
		FileSizeCategory catEmpty(wstring(L"1MB - 50MB"), maxSize);
		fileSizeCategories.push_back(catEmpty); }

		{ LARGE_INTEGER maxSize;
		maxSize.QuadPart = 1000000000; // 1GB
		FileSizeCategory catEmpty(wstring(L"50MB - 1GB"), maxSize);
		fileSizeCategories.push_back(catEmpty); }

		{ LARGE_INTEGER maxSize;
		maxSize.QuadPart = 10000000000; // 10GB
		FileSizeCategory catEmpty(wstring(L"1GB - 10GB"), maxSize);
		fileSizeCategories.push_back(catEmpty); }

		{ LARGE_INTEGER maxSize;
		maxSize.QuadPart = 1000000000000; // 1TB
		FileSizeCategory catEmpty(wstring(L"10GB - 1TB"), maxSize);
		fileSizeCategories.push_back(catEmpty); }
	}

	void updateFileSizeCategory(LARGE_INTEGER size) {
		for (FileSizeCategory& cat : fileSizeCategories) {
			if (size.QuadPart <= cat.maxSize.QuadPart) {
				cat.count.QuadPart++;
				break;
			}
		}
	}
} Tree;

void gather(Tree& t, Node* n) {

	WIN32_FIND_DATA ffd;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	n->size.QuadPart = 0;

	wstring base = n->getPath();
	wstring regex = base;
	regex.append(L"\\*");
	LPCWSTR nName = (LPCWSTR)regex.c_str();
 	hFind = FindFirstFile(nName, &ffd);

	if (INVALID_HANDLE_VALUE == hFind) {
		return;
	}

	do {

		if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) {
			continue;
		}

		Node* newNode = new Node(wstring(ffd.cFileName), n);

		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if(DEBUG)
				_tprintf(TEXT("dir %s\n"), ffd.cFileName);
			newNode->isDir = true;
			gather(t, newNode);
			t.numDirs.QuadPart++;
			if (DEBUG)
				_tprintf(TEXT(" -- dir size %lld bytes\n"), newNode->size.QuadPart);
		}
		else
		{
			newNode->isDir = false;
			newNode->size.LowPart = ffd.nFileSizeLow;
			newNode->size.HighPart = ffd.nFileSizeHigh;
			t.numFiles.QuadPart++;
			t.updateFileSizeCategory(newNode->size);
			if (DEBUG)
				_tprintf(TEXT("file %s %lld bytes\n"), ffd.cFileName, newNode->size.QuadPart);
		}

		n->size.QuadPart += newNode->size.QuadPart;
		n->files.insert(pair<LONGLONG, Node*>(newNode->size.QuadPart, newNode));

	} while(FindNextFile(hFind, &ffd) != 0);

	DWORD dwError = GetLastError();
	if (dwError != ERROR_NO_MORE_FILES)
	{
		_tprintf(TEXT("error: could not get directory listing correctly"));
	}

	FindClose(hFind);
}

void log(wstring str) {

}

void print_scr(wstring str){

}

wstring getHumanReadableSize(LARGE_INTEGER i) {
	WCHAR buff[100];

	if (i.QuadPart < 1000LL) {
		swprintf(buff, L"%i Bytes", i);
	}
	else if (i.QuadPart < 1000000LL) {
		swprintf(buff, L"%i KB", i.QuadPart / 1000LL);
	}
	else if (i.QuadPart < 1000000000LL) {
		swprintf(buff, L"%i MB", i.QuadPart / 1000000LL);
	}
	else if (i.QuadPart < 1000000000000LL) {
		swprintf(buff, L"%i GB", i.QuadPart / 1000000000LL);
	}
	else if (i.QuadPart < 1000000000000000LL) {
		swprintf(buff, L"%i TB", i.QuadPart / 1000000000000LL);
	}

	return buff;
}

bool inspect(Node& n) {
	int relativeFileNum;
	int absoluteFileNum = 0;
	char choice;
	size_t numFiles = n.files.size();

	Node ** nodeArr = (Node**) operator new[] ( numFiles * sizeof(Node*) );
	int i = 0;
	for (pair<LONGLONG, Node*> pair : n.files) {
		nodeArr[i++] = pair.second;
	}

	bool isRoot = n.parent == NULL;

	while (true) {
		
		wcout << n.getPath() << " - " << getHumanReadableSize(n.size) << endl;
		for (i = absoluteFileNum, relativeFileNum = 1; i < numFiles && i < absoluteFileNum + 9; i++, relativeFileNum++) {
			Node* node = nodeArr[i];
			wcout << " " << relativeFileNum << ") " << (node->isDir ? "[d] " : "") << node->name << " - " << getHumanReadableSize(node->size) << endl;
		}
		int remaining = numFiles - (absoluteFileNum + relativeFileNum);
		if (remaining > 0) {
			wcout << "... " << remaining << " more" << endl;
		}
		wcout << "#: inspect item, w: page up, s: page down, a: parent, d: child, x: main menu" << endl;
		cin >> choice;
		cin.clear();
		cin.ignore(10000, '\n');

		if (choice >= '1' && choice <= '9') {
			int targetFileNum = choice - 48;
			Node* nodeChoice = nodeArr[absoluteFileNum + targetFileNum - 1];
			if (nodeChoice->files.size() > 0) {
				if (!inspect(*nodeChoice)) {
					return false;
				}
			}
			continue;
		}

		switch (choice) {
			case 'w':
				if (absoluteFileNum >= 9) {
					absoluteFileNum -= 9;
				}
				break;
			case 's':
				if (absoluteFileNum + 9 < numFiles ) {
					absoluteFileNum += 9;
				}
				break;
			case 'a':
				if (isRoot)
					break;
				return true; // inspect parent
			case 'd':
				break;
			case 'x':
				return false; // stop inspecting
			default:
				break;
		}
	}

	delete[] nodeArr;
}

void menu() {

	int choice=0;
	wstring rootDir;

	while (true) {
		cout << "Bit Ruler" << endl;
		cout << "1) measure bits" << endl;
		cout << "2) exit" << endl;

		// validate a number was given
		if (!(cin >> choice)) {
			choice = 0;
		}
		cin.clear();
		cin.ignore(10000, '\n');

		switch (choice) {
			case 1: {
				cout << "Which directory do you want to measure? (for example C:\\)" << endl;
				wcin >> rootDir;
				wcin.clear();
				wcin.ignore(10000, '\n');
				Node* root = new Node(rootDir, NULL);
				Tree t(root);
				gather(t, root);

				_tprintf(TEXT("\n---- summary ----\n"));
				_tprintf(TEXT("size: %s\n"), getHumanReadableSize(root->size).c_str());
				_tprintf(TEXT("# files: %lld\n"), t.numFiles.QuadPart);
				_tprintf(TEXT("# directories: %lld\n"), t.numDirs.QuadPart);

				for (FileSizeCategory cat : t.fileSizeCategories) {
					_tprintf(TEXT("# %s FILES: %lld\n"), cat.name.c_str(), cat.count.QuadPart);
				}
				_tprintf(TEXT("-----------------\n\n"));

				inspect(*root);

				delete root;
				break;
			}

			case 2:
				return;

			default: {
				cout << "ERROR: Invalid choice" << endl;
				break;
			}			
		}
	}
}

int main()
{
	menu();
    return 0;
}

