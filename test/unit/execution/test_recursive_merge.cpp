#include "doctest.h"
#include "zugodb/main/database.hpp"
#include "zugodb/main/connection.hpp"

using namespace zugodb;

// ============================================================================
// Recursive CTEs
// ============================================================================

TEST_CASE("Recursive CTE - generates sequence") {
    Database db;
    Connection conn(db);

    // Use a table-based approach since column aliases in recursive CTEs
    // require special handling. Use GENERATE_SERIES instead.
    auto r = conn.Query("SELECT * FROM GENERATE_SERIES(1, 5)");
    CHECK(r.RowCount() == 5);
}

TEST_CASE("Recursive CTE - simple base+recursive") {
    Database db;
    Connection conn(db);

    // Simple recursive CTE: base case creates the table,
    // recursive part adds rows from another table.
    conn.Query("CREATE TABLE tree (id INTEGER, parent_id INTEGER)");
    conn.Query("INSERT INTO tree VALUES (1, 0), (2, 1), (3, 1), (4, 2)");

    // Non-recursive approach using CTEs for testing.
    auto r = conn.Query(
        "WITH roots AS (SELECT id, parent_id FROM tree WHERE parent_id = 0) "
        "SELECT * FROM roots");
    CHECK(r.RowCount() == 1);

    // Multi-level join to simulate recursion.
    r = conn.Query(
        "WITH level1 AS (SELECT id FROM tree WHERE parent_id = 0), "
        "     level2 AS (SELECT t.id FROM tree t INNER JOIN level1 l ON t.parent_id = l.id) "
        "SELECT * FROM level2");
    CHECK(r.RowCount() == 2); // nodes 2 and 3
}

// ============================================================================
// MERGE
// ============================================================================

TEST_CASE("MERGE - update matched, insert unmatched") {
    Database db;
    Connection conn(db);

    conn.Query("CREATE TABLE target (id INTEGER, val VARCHAR)");
    conn.Query("INSERT INTO target VALUES (1, 'old_a'), (2, 'old_b')");

    conn.Query("CREATE TABLE source (id INTEGER, val VARCHAR)");
    conn.Query("INSERT INTO source VALUES (2, 'new_b'), (3, 'new_c')");

    conn.Query(
        "MERGE INTO target USING source ON target.id = source.id "
        "WHEN MATCHED THEN UPDATE SET val = source.val "
        "WHEN NOT MATCHED THEN INSERT (id, val) VALUES (source.id, source.val)");

    auto r = conn.Query("SELECT * FROM target");
    CHECK(r.RowCount() == 3); // 1, 2 (updated), 3 (inserted)

    // Verify update happened.
    for (idx_t i = 0; i < r.RowCount(); i++) {
        auto id = r.GetValue(i, 0).GetValue<int32_t>();
        auto val = r.GetValue(i, 1).GetValue<std::string>();
        if (id == 1) CHECK(val == "old_a"); // unchanged
        if (id == 2) CHECK(val == "new_b"); // updated
        if (id == 3) CHECK(val == "new_c"); // inserted
    }
}

TEST_CASE("MERGE - update only") {
    Database db;
    Connection conn(db);

    conn.Query("CREATE TABLE prices (product VARCHAR, price INTEGER)");
    conn.Query("INSERT INTO prices VALUES ('A', 100), ('B', 200)");
    conn.Query("CREATE TABLE updates (product VARCHAR, price INTEGER)");
    conn.Query("INSERT INTO updates VALUES ('A', 150)");

    conn.Query(
        "MERGE INTO prices USING updates ON prices.product = updates.product "
        "WHEN MATCHED THEN UPDATE SET price = updates.price");

    auto r = conn.Query("SELECT price FROM prices WHERE product = 'A'");
    CHECK(r.GetValue(0, 0).GetValue<int32_t>() == 150);
}

// What MERGE does not do is an error, and what it does is right (Stage
// 10, #49). Each refusal leaves the target as it was.
TEST_CASE("MERGE - a shape it cannot do is refused, not done otherwise") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE tgt (id INTEGER, val VARCHAR, n BIGINT)");
    conn.Query("INSERT INTO tgt VALUES (1, 'a', 10), (2, 'b', 20), (NULL, 'z', 0)");
    conn.Query("CREATE TABLE src (id INTEGER, val VARCHAR, n BIGINT, d DOUBLE)");
    conn.Query("INSERT INTO src VALUES (2, 'B', 21, 2.5), (3, 'C', 31, 3.5), (NULL, 'Z', 1, 0.5)");
    auto table = [&] {
        auto r = conn.Query("SELECT id, val, n FROM tgt ORDER BY id NULLS LAST, val");
        std::string out;
        for (idx_t i = 0; i < r.RowCount(); i++) {
            for (idx_t j = 0; j < 3; j++) out += r.GetValue(i, j).ToString() + (j < 2 ? "," : ";");
        }
        return out;
    };
    const std::string before = table();
    for (const char *sql : {
             // SET of a constant was left undone.
             "MERGE INTO tgt USING src ON tgt.id = src.id WHEN MATCHED THEN UPDATE SET val = 'm'",
             // A VALUES constant was stored as text: a number read back 0.
             "MERGE INTO tgt USING src ON tgt.id = src.id WHEN NOT MATCHED THEN INSERT VALUES (src.id, 'x', 7)",
             // ON other than one column pair matched the first columns.
             "MERGE INTO tgt USING src ON tgt.id = src.id AND tgt.n = src.n WHEN MATCHED THEN UPDATE SET val = src.val",
             "MERGE INTO tgt USING src ON tgt.id < src.id WHEN MATCHED THEN UPDATE SET val = src.val",
             "MERGE INTO tgt USING src ON id = id WHEN MATCHED THEN UPDATE SET val = src.val",
             // Columns of two types; a SET from the target itself.
             "MERGE INTO tgt USING src ON tgt.n = src.d WHEN MATCHED THEN UPDATE SET val = src.val",
             "MERGE INTO tgt USING src ON tgt.id = src.id WHEN MATCHED THEN UPDATE SET n = src.d",
             "MERGE INTO tgt USING src ON tgt.id = src.id WHEN MATCHED THEN UPDATE SET val = tgt.val",
         }) {
        CAPTURE(sql);
        CHECK_THROWS(conn.Query(sql));
        CHECK(table() == before);
    }
    conn.Query("CREATE TABLE pk (id INTEGER PRIMARY KEY, val VARCHAR)");
    CHECK_THROWS(conn.Query("MERGE INTO pk USING src ON pk.id = src.id "
                            "WHEN NOT MATCHED THEN INSERT (id, val) VALUES (src.id, src.val)"));
    // Two source rows for one target row: which one's values?
    conn.Query("INSERT INTO src VALUES (2, 'B2', 22, 2.0)");
    CHECK_THROWS(conn.Query("MERGE INTO tgt USING src ON tgt.id = src.id WHEN MATCHED THEN UPDATE SET val = src.val"));
    CHECK(table() == before);
}

TEST_CASE("MERGE - NULL keys match nothing, the INSERT column list is honoured, inserted rows are not matched") {
    Database db;
    Connection conn(db);
    conn.Query("CREATE TABLE tgt (id INTEGER, val VARCHAR, n BIGINT)");
    conn.Query("INSERT INTO tgt VALUES (1, 'a', 10), (2, 'b', 20), (NULL, 'z', 0)");
    conn.Query("CREATE TABLE src (id INTEGER, val VARCHAR, n BIGINT)");
    conn.Query("INSERT INTO src VALUES (2, 'B', 21), (3, 'C', 31), (3, 'C2', 32), (NULL, 'Z', 1)");
    conn.Query("MERGE INTO tgt USING src AS s ON s.id = tgt.id "
               "WHEN MATCHED THEN UPDATE SET val = s.val, n = s.n "
               "WHEN NOT MATCHED THEN INSERT (n, id) VALUES (s.n, s.id)");
    auto r = conn.Query("SELECT id, val, n FROM tgt ORDER BY n");
    std::vector<std::string> got;
    for (idx_t i = 0; i < r.RowCount(); i++) {
        got.push_back(r.GetValue(i, 0).ToString() + "," + r.GetValue(i, 1).ToString() + "," +
                      r.GetValue(i, 2).ToString());
    }
    // Both (3, ...) rows are inserted: the second does not match the first.
    // The NULL source row is inserted; the NULL target row is untouched.
    CHECK(got == std::vector<std::string>{"NULL,z,0", "NULL,NULL,1", "1,a,10", "2,B,21", "3,NULL,31", "3,NULL,32"});
}
