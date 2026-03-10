CREATE TABLE IF NOT EXISTS canvas (
    x INTEGER NOT NULL,
    y INTEGER NOT NULL,
    color VARCHAR(7) NOT NULL DEFAULT '#FFFFFF',
    PRIMARY KEY (x, y)
);

INSERT INTO
    canvas (x, y)
SELECT x, y
FROM
    generate_series (0, 49) AS x
    CROSS JOIN generate_series (0, 49) AS y ON CONFLICT DO NOTHING;

CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    password_hash TEXT NOT NULL
);