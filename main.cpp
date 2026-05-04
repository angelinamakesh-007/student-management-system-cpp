#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <cctype>
#include <cstring>

using namespace std;

// ================================================================
//  LAYOUT CONSTANTS
// ================================================================
static const char* DATA_FILE  = "students.dat";
static const char* TEMP_FILE  = "students_temp.dat";
static const int   NAME_LEN   = 60;

// All horizontal rules (line / thickLine) are:
//   "  " + COL_WIDTH chars  ->  62 visual characters wide
static const int COL_WIDTH = 60;

// Box rows are:
//   "  " + '|' + BOX_INNER chars + '|'  ->  62 visual characters wide
//   (same visual width as line/thickLine for perfect alignment)
static const int BOX_INNER = 60;

// Table column widths - must sum to COL_WIDTH (60)
static const int COL_ROLL  =  6;   //  6
static const int COL_NAME  = 28;   // 34
static const int COL_MARKS = 10;   // 44
static const int COL_GRADE =  8;   // 52
static const int COL_PAD   =  8;   // 60

// ================================================================
//  On-disk POD  (binary file layout - never changes)
// ================================================================
struct DiskRecord {
    int   rollNo;
    char  name[NAME_LEN];
    float marks;
};

// ================================================================
//  In-memory student  (safe std::string)
// ================================================================
struct StudentRecord {
    int    rollNo { 0 };
    string name;
    float  marks  { 0.0f };

    DiskRecord toDisk() const {
        DiskRecord d;
        d.rollNo = rollNo;
        d.marks  = marks;
        strncpy(d.name, name.c_str(), NAME_LEN - 1);
        d.name[NAME_LEN - 1] = '\0';
        return d;
    }

    static StudentRecord fromDisk(const DiskRecord& d) {
        StudentRecord s;
        s.rollNo = d.rollNo;
        s.marks  = d.marks;
        char safe[NAME_LEN + 1];
        memcpy(safe, d.name, NAME_LEN);
        safe[NAME_LEN] = '\0';
        s.name = safe;
        return s;
    }

    string grade() const {
        if (marks >= 90.0f) return "A+";
        if (marks >= 80.0f) return "A";
        if (marks >= 70.0f) return "B";
        if (marks >= 60.0f) return "C";
        if (marks >= 50.0f) return "D";
        return "F";
    }
};

// ================================================================
//  UI  - every element on the same 60-char grid
// ================================================================
namespace UI {

    // "  " + COL_WIDTH dashes  ->  62 chars wide
    void line() {
        cout << "  " << string(COL_WIDTH, '-') << '\n';
    }

    // "  " + COL_WIDTH equals  ->  62 chars wide
    void thickLine() {
        cout << "  " << string(COL_WIDTH, '=') << '\n';
    }

    // Horizontally center text within a COL_WIDTH field, "  " indent
    void centeredText(const string& text) {
        int pad = max(0, (COL_WIDTH - (int)text.size()) / 2);
        cout << "  " << string(pad, ' ') << text << '\n';
    }

    void gap() { cout << '\n'; }

    // Status lines - consistent tag width so text always starts at the same column
    void success(const string& msg) { cout << "\n  [ OK ]  " << msg << '\n'; }
    void error  (const string& msg) { cout << "\n  [ !! ]  " << msg << '\n'; }
    void info   (const string& msg) { cout << "\n  [ >> ]  " << msg << '\n'; }

    // -- Box drawing helpers --------------------------------------
    // All box elements are 62 visual chars wide (same as line/thickLine):
    //   "  " + '+' + BOX_INNER('-' or '=') + '+'
    //   "  " + '|' + BOX_INNER(content)    + '|'
    // Using only plain ASCII: '+', '-', '=', '|' - renders identically
    // on every terminal, compiler, and OS without any encoding issues.

    void boxTop() {
        cout << "  +" << string(BOX_INNER, '-') << "+\n";
    }
    void boxDivider() {
        cout << "  +" << string(BOX_INNER, '=') << "+\n";
    }
    void boxBottom() {
        cout << "  +" << string(BOX_INNER, '-') << "+\n";
    }

    // One box row: content area = BOX_INNER - 2 = 58 chars (1 space each side)
    void boxRow(const string& text, bool center = false) {
        const int inner = BOX_INNER - 2;          // 58 usable chars
        string cell;
        if (center) {
            int pad = max(0, (inner - (int)text.size()) / 2);
            cell = string(pad, ' ') + text;
        } else {
            cell = " " + text;                    // 1-space left margin
        }
        // Pad or trim to exactly `inner` chars so the closing '|' always aligns
        if ((int)cell.size() < inner)
            cell += string(inner - (int)cell.size(), ' ');
        else if ((int)cell.size() > inner)
            cell = cell.substr(0, inner);

        cout << "  |" << cell << "|\n";
    }

    // -- Section header above a feature (replaces the old gap+thickLine+title+line) --
    void sectionHeader(const string& title) {
        gap();
        thickLine();
        centeredText(title);
        line();
    }

    // -- Table -----------------------------------------------------
    void tableHeader() {
        gap();
        thickLine();
        cout << "  "
             << left << setw(COL_ROLL)  << "Roll"
             << left << setw(COL_NAME)  << "Name"
             << left << setw(COL_MARKS) << "Marks"
             << left << setw(COL_GRADE) << "Grade"
             << '\n';
        line();
    }

    void tableRow(const StudentRecord& s) {
        // Truncate name if it would overflow the column
        string displayName = s.name;
        if ((int)displayName.size() >= COL_NAME)
            displayName = displayName.substr(0, COL_NAME - 1);

        cout << "  "
             << left  << setw(COL_ROLL)  << s.rollNo
             << left  << setw(COL_NAME)  << displayName
             << fixed << setprecision(1)
             << left  << setw(COL_MARKS) << s.marks
             << left  << setw(COL_GRADE) << s.grade()
             << '\n';
    }

    void tableFooter(int count) {
        thickLine();
        cout << "  Total Records : " << count << '\n';
        gap();
    }

} // namespace UI

// ================================================================
//  Input  - validated, EOF-safe, no infinite loops
// ================================================================
namespace Input {

    void flush() {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    bool getPositiveInt(const string& prompt, int& out) {
        while (true) {
            cout << prompt;
            if (cin.eof()) return false;
            if (cin >> out && out > 0) { flush(); return true; }
            UI::error("Please enter a valid positive integer.");
            flush();
        }
    }

    bool getMarks(const string& prompt, float& out) {
        while (true) {
            cout << prompt;
            if (cin.eof()) return false;
            if (cin >> out && out >= 0.0f && out <= 100.0f) { flush(); return true; }
            UI::error("Marks must be a number between 0 and 100.");
            flush();
        }
    }

    // Returns false on EOF; trims leading and trailing whitespace
    bool getName(const string& prompt, string& out) {
        while (true) {
            cout << prompt;
            if (cin.eof()) return false;
            string ln;
            if (!getline(cin, ln)) return false;

            auto start = ln.find_first_not_of(" \t\r\n");
            if (start == string::npos) { UI::error("Name cannot be empty."); continue; }
            auto end = ln.find_last_not_of(" \t\r\n");
            out = ln.substr(start, end - start + 1);

            if ((int)out.size() >= NAME_LEN) {
                out = out.substr(0, NAME_LEN - 1);
                UI::info("Name was truncated to " + to_string(NAME_LEN - 1) + " characters.");
            }
            return true;
        }
    }

    // Returns -1 on bad input or silent EOF
    int getMenuChoice(const string& prompt) {
        if (!prompt.empty()) cout << prompt;
        int choice;
        if (cin.eof()) return -1;
        if (cin >> choice) { flush(); return choice; }
        flush();
        return -1;
    }

    bool confirm(const string& prompt) {
        char ch;
        cout << prompt << " (y/n): ";
        if (!(cin >> ch)) { flush(); return false; }
        flush();
        return (tolower(static_cast<unsigned char>(ch)) == 'y');
    }

} // namespace Input

// ================================================================
//  FileIO  - atomic writes via temp-file rename
// ================================================================
namespace FileIO {

    vector<StudentRecord> loadAll() {
        vector<StudentRecord> records;
        ifstream file(DATA_FILE, ios::binary);
        if (!file) return records;
        DiskRecord d;
        while (file.read(reinterpret_cast<char*>(&d), sizeof(d)))
            records.push_back(StudentRecord::fromDisk(d));
        return records;
    }

    // Write to TEMP_FILE first; rename over DATA_FILE only on success.
    // Original data is untouched if anything goes wrong.
    bool saveAll(const vector<StudentRecord>& records) {
        {
            ofstream tmp(TEMP_FILE, ios::binary | ios::trunc);
            if (!tmp) {
                UI::error("Could not open temporary file for writing.");
                return false;
            }
            for (const StudentRecord& s : records) {
                DiskRecord d = s.toDisk();
                if (!tmp.write(reinterpret_cast<const char*>(&d), sizeof(d))) {
                    UI::error("Write error - original data has not been changed.");
                    return false;
                }
            }
        } // tmp closed by RAII here

        if (remove(DATA_FILE) != 0 && errno != ENOENT) {
            UI::error("Could not replace the data file.");
            remove(TEMP_FILE);
            return false;
        }
        if (rename(TEMP_FILE, DATA_FILE) != 0) {
            UI::error("Could not finalise save - please check file permissions.");
            return false;
        }
        return true;
    }

    bool hasRecords() {
        ifstream file(DATA_FILE, ios::binary | ios::ate);
        if (!file) return false;
        return file.tellg() >= static_cast<streamoff>(sizeof(DiskRecord));
    }

} // namespace FileIO

// ================================================================
//  StudentManager
// ================================================================
class StudentManager {
private:

    static bool rollExists(const vector<StudentRecord>& records, int roll) {
        return any_of(records.begin(), records.end(),
            [roll](const StudentRecord& s){ return s.rollNo == roll; });
    }

    static bool partialMatch(const string& haystack, const string& needle) {
        string h = haystack, n = needle;
        transform(h.begin(), h.end(), h.begin(),
            [](unsigned char c){ return tolower(c); });
        transform(n.begin(), n.end(), n.begin(),
            [](unsigned char c){ return tolower(c); });
        return h.find(n) != string::npos;
    }

public:

    // ------------------------------------------------------------
    //  ADD STUDENT
    // ------------------------------------------------------------
    void addStudent() {
        UI::sectionHeader("ADD NEW STUDENT");

        auto records = FileIO::loadAll();
        StudentRecord s;

        while (true) {
            if (!Input::getPositiveInt("  Roll Number   : ", s.rollNo)) return;
            if (!rollExists(records, s.rollNo)) break;
            UI::error("Roll Number " + to_string(s.rollNo) + " already exists. Try another.");
        }

        if (!Input::getName ("  Full Name     : ", s.name))   return;
        if (!Input::getMarks("  Marks (0-100) : ", s.marks))  return;

        records.push_back(s);
        if (FileIO::saveAll(records))
            UI::success("Student \"" + s.name + "\" added successfully.");
    }

    // ------------------------------------------------------------
    //  DISPLAY ALL
    // ------------------------------------------------------------
    void displayAll() const {
        if (!FileIO::hasRecords()) {
            UI::info("No records found. Please add some students first.");
            return;
        }
        const auto records = FileIO::loadAll();
        UI::gap();
        UI::thickLine();
        UI::centeredText("ALL STUDENT RECORDS");
        UI::tableHeader();
        for (const auto& s : records) UI::tableRow(s);
        UI::tableFooter(static_cast<int>(records.size()));
    }

    // ------------------------------------------------------------
    //  UPDATE STUDENT
    // ------------------------------------------------------------
    void updateStudent() {
        if (!FileIO::hasRecords()) { UI::info("No records to update."); return; }

        UI::sectionHeader("UPDATE STUDENT RECORD");

        int targetRoll;
        if (!Input::getPositiveInt("  Enter Roll Number to update : ", targetRoll)) return;

        auto records = FileIO::loadAll();
        auto it = find_if(records.begin(), records.end(),
            [targetRoll](const StudentRecord& s){ return s.rollNo == targetRoll; });

        if (it == records.end()) {
            UI::error("No student found with Roll Number " + to_string(targetRoll) + ".");
            return;
        }

        UI::gap();
        cout << "  Current Record\n";
        cout << "    Name  : " << it->name << '\n';
        cout << "    Marks : " << fixed << setprecision(1) << it->marks << "\n\n";

        string newName;
        float  newMarks;
        if (!Input::getName ("  New Name     : ", newName))   return;
        if (!Input::getMarks("  New Marks    : ", newMarks))  return;

        it->name  = newName;
        it->marks = newMarks;

        if (FileIO::saveAll(records))
            UI::success("Record updated successfully.");
    }

    // ------------------------------------------------------------
    //  DELETE STUDENT
    // ------------------------------------------------------------
    void deleteStudent() {
        if (!FileIO::hasRecords()) { UI::info("No records to delete."); return; }

        UI::sectionHeader("DELETE STUDENT RECORD");

        int targetRoll;
        if (!Input::getPositiveInt("  Enter Roll Number to delete : ", targetRoll)) return;

        auto records = FileIO::loadAll();
        auto it = find_if(records.begin(), records.end(),
            [targetRoll](const StudentRecord& s){ return s.rollNo == targetRoll; });

        if (it == records.end()) {
            UI::error("No student found with Roll Number " + to_string(targetRoll) + ".");
            return;
        }

        UI::gap();
        cout << "  Record to Delete\n";
        cout << "    Roll  : " << it->rollNo << '\n';
        cout << "    Name  : " << it->name   << '\n';
        cout << "    Marks : " << fixed << setprecision(1) << it->marks << '\n';

        if (!Input::confirm("\n  Confirm delete?")) {
            UI::info("Delete cancelled.");
            return;
        }

        records.erase(it);
        if (FileIO::saveAll(records))
            UI::success("Student with Roll Number " + to_string(targetRoll) + " deleted.");
    }

    // ------------------------------------------------------------
    //  SEARCH
    // ------------------------------------------------------------
    void searchMenu() {
        if (!FileIO::hasRecords()) { UI::info("No records to search."); return; }

        UI::sectionHeader("SEARCH STUDENT");
        cout << "  1.   Search by Roll Number\n";
        cout << "  2.   Search by Name  (partial match)\n";
        UI::line();

        const int choice = Input::getMenuChoice("  Your choice : ");
        if      (choice == 1) searchByRoll();
        else if (choice == 2) searchByName();
        else UI::error("Invalid option. Please enter 1 or 2.");
    }

    void searchByRoll() {
        int roll;
        if (!Input::getPositiveInt("  Enter Roll Number : ", roll)) return;

        const auto records = FileIO::loadAll();
        for (const auto& s : records) {
            if (s.rollNo == roll) {
                UI::tableHeader();
                UI::tableRow(s);
                UI::tableFooter(1);
                return;
            }
        }
        UI::error("No student found with Roll Number " + to_string(roll) + ".");
    }

    void searchByName() {
        string keyword;
        if (!Input::getName("  Enter Name (or part of it) : ", keyword)) return;

        const auto records = FileIO::loadAll();
        vector<StudentRecord> results;
        for (const auto& s : records)
            if (partialMatch(s.name, keyword)) results.push_back(s);

        if (!results.empty()) {
            UI::tableHeader();
            for (const auto& s : results) UI::tableRow(s);
            UI::tableFooter(static_cast<int>(results.size()));
        } else {
            UI::error("No student found matching \"" + keyword + "\".");
        }
    }

    // ------------------------------------------------------------
    //  SORT  (with optional persist)
    // ------------------------------------------------------------
    void sortMenu() {
        if (!FileIO::hasRecords()) { UI::info("No records to sort."); return; }

        UI::sectionHeader("SORT STUDENTS");
        cout << "  1.   By Marks       - Ascending   (lowest first)\n";
        cout << "  2.   By Marks       - Descending  (highest first)\n";
        cout << "  3.   By Roll Number - Ascending\n";
        cout << "  4.   By Name        - Alphabetical  (A to Z)\n";
        UI::line();

        const int choice = Input::getMenuChoice("  Your choice : ");
        auto records = FileIO::loadAll();
        string sortLabel;

        switch (choice) {
            case 1:
                sort(records.begin(), records.end(),
                    [](const StudentRecord& a, const StudentRecord& b){
                        return a.marks < b.marks; });
                sortLabel = "SORTED BY MARKS - ASCENDING";
                break;
            case 2:
                sort(records.begin(), records.end(),
                    [](const StudentRecord& a, const StudentRecord& b){
                        return a.marks > b.marks; });
                sortLabel = "SORTED BY MARKS - DESCENDING";
                break;
            case 3:
                sort(records.begin(), records.end(),
                    [](const StudentRecord& a, const StudentRecord& b){
                        return a.rollNo < b.rollNo; });
                sortLabel = "SORTED BY ROLL NUMBER";
                break;
            case 4:
                sort(records.begin(), records.end(),
                    [](const StudentRecord& a, const StudentRecord& b){
                        return a.name < b.name; });
                sortLabel = "SORTED BY NAME  (A TO Z)";
                break;
            default:
                UI::error("Invalid option. Please enter 1 to 4.");
                return;
        }

        UI::gap();
        UI::thickLine();
        UI::centeredText(sortLabel);
        UI::tableHeader();
        for (const auto& s : records) UI::tableRow(s);
        UI::tableFooter(static_cast<int>(records.size()));

        if (Input::confirm("  Save this sorted order to file?")) {
            if (FileIO::saveAll(records))
                UI::success("Sorted order saved to file.");
        }
    }

    // ------------------------------------------------------------
    //  STATISTICS
    // ------------------------------------------------------------
    void showStats() const {
        if (!FileIO::hasRecords()) {
            UI::info("No records available for statistics.");
            return;
        }
        const auto records = FileIO::loadAll();
        if (records.empty()) { UI::info("No records available."); return; }

        float total   = 0.0f;
        float highest = records[0].marks;
        float lowest  = records[0].marks;
        string topName    = records[0].name;
        string bottomName = records[0].name;
        int ap=0, a=0, b=0, c=0, d=0, f=0;

        for (const auto& s : records) {
            total += s.marks;
            if (s.marks > highest) { highest = s.marks; topName    = s.name; }
            if (s.marks < lowest)  { lowest  = s.marks; bottomName = s.name; }
            if      (s.marks >= 90) ap++;
            else if (s.marks >= 80) a++;
            else if (s.marks >= 70) b++;
            else if (s.marks >= 60) c++;
            else if (s.marks >= 50) d++;
            else                    f++;
        }
        float avg = total / static_cast<float>(records.size());

        UI::gap();
        UI::thickLine();
        UI::centeredText("CLASS STATISTICS");
        UI::line();
        cout << fixed << setprecision(1);
        cout << "  Total Students   : " << records.size() << '\n';
        cout << "  Class Average    : " << avg             << '\n';
        cout << "  Highest Marks    : " << highest << "  (" << topName    << ")\n";
        cout << "  Lowest  Marks    : " << lowest  << "  (" << bottomName << ")\n";
        UI::line();
        cout << "  Grade Distribution :\n";
        cout << "    A+  (90 - 100) : " << ap << " student(s)\n";
        cout << "    A   (80 -  89) : " << a  << " student(s)\n";
        cout << "    B   (70 -  79) : " << b  << " student(s)\n";
        cout << "    C   (60 -  69) : " << c  << " student(s)\n";
        cout << "    D   (50 -  59) : " << d  << " student(s)\n";
        cout << "    F   ( 0 -  49) : " << f  << " student(s)\n";
        UI::thickLine();
    }

    // ------------------------------------------------------------
    //  EXPORT
    // ------------------------------------------------------------
    void exportMenu() const {
        if (!FileIO::hasRecords()) { UI::info("No records to export."); return; }

        UI::sectionHeader("EXPORT RECORDS");
        cout << "  1.   Export as Plain Text  (.txt)\n";
        cout << "  2.   Export as CSV         (.csv)\n";
        UI::line();

        const int choice = Input::getMenuChoice("  Your choice : ");
        if      (choice == 1) exportTxt();
        else if (choice == 2) exportCsv();
        else UI::error("Invalid option. Please enter 1 or 2.");
    }

    void exportTxt() const {
        const string filename = "students_export.txt";
        ofstream out(filename);
        if (!out) { UI::error("Could not create \"" + filename + "\"."); return; }

        const auto records = FileIO::loadAll();
        const int W = 62;

        out << string(W, '=') << "\n";
        out << "  SMART STUDENT RECORD MANAGER - Export\n";
        out << string(W, '=') << "\n";
        out << left << setw(6)  << "Roll"
                    << setw(30) << "Name"
                    << setw(10) << "Marks"
                    << setw(8)  << "Grade" << "\n";
        out << string(W, '-') << "\n";
        for (const auto& s : records) {
            out << left << setw(6)  << s.rollNo
                        << setw(30) << s.name
                        << fixed << setprecision(1)
                        << setw(10) << s.marks
                        << setw(8)  << s.grade() << "\n";
        }
        out << string(W, '=') << "\n";
        out << "  Total Records : " << records.size() << "\n";

        UI::success("Exported " + to_string(records.size())
                    + " record(s) to \"" + filename + "\".");
    }

    void exportCsv() const {
        const string filename = "students_export.csv";
        ofstream out(filename);
        if (!out) { UI::error("Could not create \"" + filename + "\"."); return; }

        const auto records = FileIO::loadAll();
        out << "Roll Number,Name,Marks,Grade\n";
        for (const auto& s : records) {
            // RFC 4180: quote fields containing commas or double-quotes
            string safeName = s.name;
            bool needsQuote = safeName.find(',') != string::npos
                           || safeName.find('"') != string::npos;
            if (needsQuote) {
                string escaped;
                for (char c : safeName)
                    escaped += (c == '"') ? string("\"\"") : string(1, c);
                safeName = "\"" + escaped + "\"";
            }
            out << s.rollNo << "," << safeName << ","
                << fixed << setprecision(1) << s.marks << ","
                << s.grade() << "\n";
        }

        UI::success("Exported " + to_string(records.size())
                    + " record(s) to \"" + filename + "\".");
    }

}; // class StudentManager

// ================================================================
//  WELCOME SCREEN
// ================================================================
void showWelcome() {
    cout << '\n';
    UI::boxTop();
    UI::boxRow("SMART STUDENT RECORD MANAGER", true);
    UI::boxDivider();
    UI::boxRow("Records are saved automatically to disk.", true);
    UI::boxBottom();
    cout << '\n';
}

// ================================================================
//  MAIN MENU
// ================================================================
void showMainMenu() {
    cout << '\n';
    UI::boxTop();
    UI::boxRow("SMART STUDENT RECORD MANAGER", true);
    UI::boxDivider();
    UI::boxRow("  1.   Add Student");
    UI::boxRow("  2.   Display All Students");
    UI::boxRow("  3.   Update Student");
    UI::boxRow("  4.   Delete Student");
    UI::boxRow("  5.   Search Student");
    UI::boxRow("  6.   Sort Students");
    UI::boxRow("  7.   Class Statistics");
    UI::boxRow("  8.   Export Records  ( TXT / CSV )");
    UI::boxRow("  9.   Exit");
    UI::boxBottom();
    cout << "  Your choice : ";
}

// ================================================================
//  MAIN
// ================================================================
int main() {
    StudentManager manager;
    showWelcome();

    int choice = -1;
    do {
        showMainMenu();
        choice = Input::getMenuChoice("");

        switch (choice) {
            case 1: manager.addStudent();    break;
            case 2: manager.displayAll();    break;
            case 3: manager.updateStudent(); break;
            case 4: manager.deleteStudent(); break;
            case 5: manager.searchMenu();    break;
            case 6: manager.sortMenu();      break;
            case 7: manager.showStats();     break;
            case 8: manager.exportMenu();    break;
            case 9:
                UI::gap();
                UI::thickLine();
                UI::centeredText("Goodbye!  All records are safely stored.");
                UI::thickLine();
                UI::gap();
                break;
            default:
                // EOF: exit silently without any technical message
                if (cin.eof()) { choice = 9; break; }
                UI::error("Invalid choice. Please enter a number from 1 to 9.");
        }

    } while (choice != 9);

    return 0;
}
