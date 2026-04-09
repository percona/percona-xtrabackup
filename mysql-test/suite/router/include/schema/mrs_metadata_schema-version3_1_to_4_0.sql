SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0;
SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0;
SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';
USE `mysql_rest_service_metadata`;
# Set schema_version to 0.0.0 to indicate an ongoing upgrade
CREATE OR REPLACE SQL SECURITY INVOKER VIEW `mysql_rest_service_metadata`.`msm_schema_version` (major, minor, patch)
AS SELECT 0, 0, 0;
# -----------------------------------------------------
# Steps to upgrade the schema to the new version
# Drop old `schema_version` VIEW as it is replaced by `msm_schema_version`
DROP VIEW IF EXISTS `mysql_rest_service_metadata`.`schema_version`;
# Apply changes to `mrs_privilege` table
UPDATE `mysql_rest_service_metadata`.`mrs_privilege`
SET service_path = '*' WHERE service_path IS NULL;
UPDATE `mysql_rest_service_metadata`.`mrs_privilege`
SET schema_path = '*' WHERE schema_path IS NULL;
UPDATE `mysql_rest_service_metadata`.`mrs_privilege`
SET object_path = '*' WHERE object_path IS NULL;
ALTER TABLE `mysql_rest_service_metadata`.`mrs_privilege`
DROP CONSTRAINT `fk_priv_on_schema_db_schema1`,
DROP CONSTRAINT `fk_priv_on_schema_service1`,
DROP CONSTRAINT `fk_priv_on_schema_db_object1`;
DELIMITER $$;
DROP TRIGGER `mysql_rest_service_metadata`.`db_schema_BEFORE_DELETE`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`db_schema_BEFORE_DELETE` BEFORE DELETE ON `db_schema` FOR EACH ROW
BEGIN
	DELETE FROM `mysql_rest_service_metadata`.`db_object` WHERE `db_schema_id` = OLD.`id`;
END$$
DROP TRIGGER `mysql_rest_service_metadata`.`db_object_BEFORE_DELETE`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`db_object_BEFORE_DELETE` BEFORE DELETE ON `db_object` FOR EACH ROW
BEGIN
    DELETE FROM `mysql_rest_service_metadata`.`mrs_db_object_row_group_security` WHERE `db_object_id` = OLD.`id`;
    DELETE FROM `mysql_rest_service_metadata`.`object` WHERE `db_object_id` = OLD.`id`;
END$$
DELIMITER ;$$
ALTER TABLE `mysql_rest_service_metadata`.`mrs_privilege`
    DROP COLUMN `service_id`,
    DROP COLUMN `db_schema_id`,
    DROP COLUMN `db_object_id`,
    CHANGE COLUMN `service_path` `service_path` VARCHAR(512) NOT NULL DEFAULT '*',
    CHANGE COLUMN `schema_path` `schema_path` VARCHAR(255) NOT NULL DEFAULT '*',
    CHANGE COLUMN `object_path` `object_path` VARCHAR(255) NOT NULL DEFAULT '*';
# Ensure an email cannot be used as user name and a user name cannot be used as email
ALTER TABLE `mysql_rest_service_metadata`.`mrs_user`
    ADD CONSTRAINT `mrs_user_no_at_symbol_in_user_name` CHECK (INSTR(name, '@') = 0),
    ADD CONSTRAINT `mrs_user_at_symbol_in_email` CHECK (INSTR(email, '@') > 0 OR email IS NULL OR email = '');
# -----------------------------------------------------
# Set the version VIEWs to the correct version
CREATE OR REPLACE SQL SECURITY INVOKER VIEW `mysql_rest_service_metadata`.`mrs_user_schema_version` (major, minor, patch) AS SELECT 4, 0, 0;
CREATE OR REPLACE SQL SECURITY INVOKER VIEW `mysql_rest_service_metadata`.`msm_schema_version` (major, minor, patch) AS SELECT 4, 0, 0;
SET SQL_MODE=@OLD_SQL_MODE;
SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS;
SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS;