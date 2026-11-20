SELECT R.b, SUM(R.g) FROM blobs as L, blobs2 AS R WHERE L.b=R.b GROUP BY R.b ORDER BY R.b
