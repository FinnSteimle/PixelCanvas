-- 1. Create the table structure
CREATE TABLE IF NOT EXISTS canvas (
    x INTEGER NOT NULL,
    y INTEGER NOT NULL,
    color VARCHAR(7) NOT NULL DEFAULT '#FFFFFF',
    PRIMARY KEY (x, y)
);

-- 2. "Seed" the board (Pre-fill all 2,500 pixels as white)
-- This makes the board "exist" before anyone even clicks.
INSERT INTO
    canvas (x, y)
SELECT x, y
FROM
    generate_series (0, 49) AS x
    CROSS JOIN generate_series (0, 49) AS y ON CONFLICT DO NOTHING