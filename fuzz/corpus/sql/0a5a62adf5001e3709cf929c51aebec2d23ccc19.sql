CREATE OR REPLACE MACRO swan_subject_info (duckSubject) AS (
    WITH
        subjectComponents AS (
            SELECT
                duckSubject AS subject,
                STRING_SPLIT(duckSubject, ':') AS parts
        )
    SELECT
        CASE
            WHEN parts[1] = 'EnumField2' THEN STRUCT_PACK(
                subject := subject,
                type := swan_MY_ENUM (parts[2])
            )
            WHEN parts[1] = 'EnumField1' THEN STRUCT_PACK(subject := subject, type := 'EnumField1'::MY_ENUM)
            WHEN REGEXP_MATCHES(
                subject,
                'duckdb.org'
            ) THEN STRUCT_PACK(
                subject := subject,
                type := 'EnumField6'::MY_ENUM
            )
            WHEN NOT REGEXP_FULL_MATCH(subject, '.+@.+\..+') THEN STRUCT_PACK(
                subject := subject,
                type := 'EnumField7'::MY_ENUM
            )
        END AS duckInfo
    FROM
        subjectComponents
);
