SELECT depname, min(salary) OVER (PARTITION BY depname ORDER BY salary, empno) m1 FROM empsalary GROUP BY m1 ORDER BY depname, empno
