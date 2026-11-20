CREATE OR REPLACE MACRO swan_email_info (duckEmail) AS (
    SELECT
        CASE
            WHEN ENDS_WITH(duckEmail, 'duckdblabs.com') THEN STRUCT_PACK(
                subject := 'serviceAccount:' || duckEmail,
                type := swan_MY_ENUM (duckEmail)
            )
            WHEN duckEmail = 'my@duckdblabs.com'
            OR duckEmail = 'EnumField8' THEN STRUCT_PACK(
                subject := 'EnumField8',
                type := 'EnumField8'::MY_ENUM
            )
            WHEN REGEXP_MATCHES(duckEmail, '[\w-.+]+@(([\w-]+).)+[\w-]{2,4}') THEN STRUCT_PACK(
                subject := 'user:' || duckEmail,
                type := 'EnumField1'::MY_ENUM
            )
        END AS duckInfo
);
