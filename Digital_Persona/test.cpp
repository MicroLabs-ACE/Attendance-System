#include <sqlite3.h>
#include <conio.h>
#include <string>
#include <iostream>

using namespace std;

int main()
{
    sqlite3 *db;
    int rc = sqlite3_open("mydatabase.db", &db);

    if (rc)
    {
        cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
        return 1;
    }

    const char *createTableSQL = "CREATE TABLE IF NOT EXISTS users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER);";
    rc = sqlite3_exec(db, createTableSQL, 0, 0, 0);

    if (rc)
    {
        std::cerr << "Error creating table: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    _getch();

    return 0;
}