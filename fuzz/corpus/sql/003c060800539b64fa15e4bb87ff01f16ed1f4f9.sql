create view v1 as select * from read_csv('{TEST_DIR}/datetest.csv', columns={'dt': date});
