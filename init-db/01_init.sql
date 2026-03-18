-- Database initialization for PixelCanvas

-- Table to store the current state of the 50x50 canvas
CREATE TABLE IF NOT EXISTS canvas (
    x INTEGER NOT NULL,
    y INTEGER NOT NULL,
    color VARCHAR(7) NOT NULL DEFAULT '#FFFFFF', -- Stores hex color codes
    PRIMARY KEY (x, y) -- Each coordinate is unique
);

-- Seed the canvas with 2500 white pixels (50x50 grid)
INSERT INTO
    canvas (x, y)
SELECT x, y
FROM
    generate_series (0, 49) AS x
    CROSS JOIN generate_series (0, 49) AS y ON CONFLICT DO NOTHING;

-- Table to store registered users and their hashed passwords
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    password_hash TEXT NOT NULL -- libsodium hashed password
);