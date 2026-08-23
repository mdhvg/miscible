--x, R"(

-- Copyright (c) 2025-2026 Madhav Goyal
-- Licensed under the GNU General Public License v3.0 (see LICENSE)

PRAGMA foreign_keys = ON;
PRAGMA recursive_triggers = ON;

CREATE TABLE IF NOT EXISTS Images (
    id          INTEGER     PRIMARY KEY             AUTOINCREMENT,
    path        TEXT        UNIQUE NOT NULL,
    filename    TEXT               NOT NULL,
    atlas_id    INTEGER     REFERENCES Atlas(id)    DEFAULT NULL,
    atlas_idx   INTEGER                             DEFAULT NULL,
    size        INTEGER                             DEFAULT 0,

    -- Filesystem dates
    mtime       INTEGER                             DEFAULT 0,
    ctime       INTEGER                             DEFAULT 0,

    -- Date added
    atime       INTEGER                             DEFAULT (unixepoch()),

    width       INTEGER                             DEFAULT 0,
    height      INTEGER                             DEFAULT 0,
    channels    INTEGER                             DEFAULT 0,
    embedding	BLOB 	                            DEFAULT NULL,
    root_dir    INTEGER     REFERENCES Dirs(id)     DEFAULT NULL,
    parent_dir  INTEGER     REFERENCES Dirs(id)     DEFAULT NULL,
    modified    INTEGER                             DEFAULT 0
);

CREATE VIRTUAL TABLE IF NOT EXISTS Image_FTS USING fts5(
    path,
    content='Images',
    content_rowid='id',
    tokenize='trigram'
);

DROP TRIGGER IF EXISTS Image_FTS_insert;
CREATE TRIGGER Image_FTS_insert AFTER INSERT ON Images BEGIN
    INSERT INTO Image_FTS(rowid, path)
    VALUES (new.id, new.path);
END;

DROP TRIGGER IF EXISTS Image_FTS_delete;
CREATE TRIGGER Image_FTS_delete AFTER DELETE ON Images BEGIN
    INSERT INTO Image_FTS(Image_FTS, rowid, path)
    VALUES('delete', old.id, old.path);
END;

DROP TRIGGER IF EXISTS Image_FTS_update;
CREATE TRIGGER Image_FTS_update AFTER UPDATE OF path ON Images BEGIN
    INSERT INTO Image_FTS(Image_FTS, rowid, path)
    VALUES('delete', old.id, old.path);

    INSERT INTO Image_FTS(rowid, path)
    VALUES(new.id, new.path);
END;

-- CREATE INDEX IF NOT EXISTS idx_imagepath ON Images(path);

CREATE TABLE IF NOT EXISTS DirSelect (
    path        TEXT    UNIQUE  NOT NULL,
    indexed     INTEGER         NOT NULL    DEFAULT 0
);

CREATE TABLE IF NOT EXISTS Dirs (
    id          INTEGER PRIMARY KEY         AUTOINCREMENT,
    path        TEXT    UNIQUE  NOT NULL,
    name        TEXT            NOT NULL,
    mtime       INTEGER                     DEFAULT 0,
    level       INTEGER                     DEFAULT 0,
    root_dir    INTEGER REFERENCES Dirs(id) DEFAULT NULL,
    parent_dir  INTEGER REFERENCES Dirs(id) DEFAULT NULL
);

CREATE INDEX IF NOT EXISTS idx_images_root ON Images(root_dir);

CREATE TABLE IF NOT EXISTS Atlas (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    atlas_path  TEXT    NOT NULL    UNIQUE
);

CREATE TABLE IF NOT EXISTS AtlasSlots (
    atlas_id    INTEGER NOT NULL    REFERENCES Atlas(id),
    atlas_idx   INTEGER NOT NULL,
    image_id    INTEGER             REFERENCES Images(id),
    PRIMARY KEY(atlas_id, atlas_idx)
);
-- )"
