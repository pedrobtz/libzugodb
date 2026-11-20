#include "doctest.h"
#include "zugodb/main/database.hpp"
#include "zugodb/main/connection.hpp"
#include "zugodb/common/exception.hpp"

using namespace zugodb;

// No transactions (design L26): BEGIN, COMMIT and ROLLBACK are refused,
// the whole batch before any of it runs. A statement after a failed one
// inside BEGIN ... COMMIT was kept, where DuckDB rolls the transaction back.
TEST_CASE("Transaction - BEGIN, COMMIT and ROLLBACK are refused") {
    Database db;
    Connection conn(db);

    conn.Query("CREATE TABLE t (x INTEGER)");
    for (const char *sql : {"BEGIN", "BEGIN TRANSACTION", "COMMIT", "COMMIT TRANSACTION",
                            "ROLLBACK", "START TRANSACTION", "END", "ABORT"}) {
        CAPTURE(sql);
        CHECK_THROWS(conn.Query(sql));
        CHECK_FALSE(conn.InTransaction());
    }
    // Nothing of a batch that holds one runs.
    CHECK_THROWS(conn.Query("INSERT INTO t VALUES (1); BEGIN; INSERT INTO t VALUES (2); COMMIT"));
    auto r = conn.Query("SELECT COUNT(*) FROM t");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 0);
}

TEST_CASE("Transaction - multiple connections") {
    Database db;
    Connection conn1(db);
    Connection conn2(db);

    conn1.Query("CREATE TABLE t (x INTEGER)");
    conn1.Query("INSERT INTO t VALUES (1)");

    // conn2 can see the data (no isolation yet - auto-commit mode).
    auto r = conn2.Query("SELECT COUNT(*) FROM t");
    CHECK(r.GetValue(0, 0).GetValue<int64_t>() == 1);
}

TEST_CASE("TransactionManager - basic lifecycle") {
    TransactionManager mgr;

    auto &txn = mgr.BeginTransaction();
    CHECK(txn.IsActive());
    CHECK(mgr.ActiveTransactionCount() == 1);

    mgr.CommitTransaction(txn);
    CHECK(mgr.ActiveTransactionCount() == 0);
}

TEST_CASE("TransactionManager - rollback") {
    TransactionManager mgr;

    auto &txn = mgr.BeginTransaction();
    mgr.RollbackTransaction(txn);
    CHECK(mgr.ActiveTransactionCount() == 0);
}

TEST_CASE("TransactionManager - multiple transactions") {
    TransactionManager mgr;

    auto &txn1 = mgr.BeginTransaction();
    auto &txn2 = mgr.BeginTransaction();
    CHECK(mgr.ActiveTransactionCount() == 2);

    mgr.CommitTransaction(txn1);
    CHECK(mgr.ActiveTransactionCount() == 1);

    mgr.CommitTransaction(txn2);
    CHECK(mgr.ActiveTransactionCount() == 0);
}

TEST_CASE("TransactionManager - timestamps increment") {
    TransactionManager mgr;

    auto &txn1 = mgr.BeginTransaction();
    auto ts1 = txn1.GetStartTimestamp();
    mgr.CommitTransaction(txn1);

    auto &txn2 = mgr.BeginTransaction();
    auto ts2 = txn2.GetStartTimestamp();
    mgr.CommitTransaction(txn2);

    CHECK(ts2 > ts1);
}
