# -----------------------------------------------------
# MySQL REST Service Metadata Schema - CREATE Script

SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0;
SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0;
SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';

# -----------------------------------------------------
# Schema mysql_rest_service_metadata
#
# Holds metadata information for the MySQL REST Service.
# -----------------------------------------------------
DROP SCHEMA IF EXISTS `mysql_rest_service_metadata`;
CREATE SCHEMA `mysql_rest_service_metadata` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;
USE `mysql_rest_service_metadata`;

# Set schema_version to 0.0.0 to indicate an ongoing creation/upgrade of the schema
CREATE SQL SECURITY INVOKER VIEW `mysql_rest_service_metadata`.`schema_version` (major, minor, patch) AS SELECT 0, 0, 0;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`url_host`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`url_host` (
  `id` BINARY(16) NOT NULL,
  `name` VARCHAR(255) NOT NULL DEFAULT '' COMMENT 'Specifies the host name of the MRS as represented in the request URLs. Example: example.com',
  `comments` VARCHAR(512) NULL,
  PRIMARY KEY (`id`),
  UNIQUE INDEX `name_UNIQUE` (`name` ASC) VISIBLE)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`service`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`service` (
  `id` BINARY(16) NOT NULL,
  `parent_id` BINARY(16) NULL,
  `url_host_id` BINARY(16) NOT NULL,
  `url_context_root` VARCHAR(255) NOT NULL DEFAULT '/mrs' COMMENT 'Specifies context root of the MRS as represented in the request URLs, default being /mrs. URL Example: https://www.example.com/mrs',
  `url_protocol` SET('HTTP', 'HTTPS') NOT NULL DEFAULT 'HTTP,HTTPS',
  `name` VARCHAR(255) NOT NULL,
  `enabled` TINYINT NOT NULL DEFAULT 1,
  `published` TINYINT NOT NULL DEFAULT 0,
  `in_development` JSON NULL COMMENT 'If not NULL, this column indicates that the REST service is currently \"in development\" and holds the name(s) of the developer(s) who is(/are) allowed to work with the service in the \"$.developers\" string array. REST services with this column not being NULL may use the same url_host+url_context_root context path as existing services. Routers only serve REST services with this column being NULL, unless they are bootstrapped with --mrs-development <user> which sets `router`.`option`->>\"$.developer\". When bootstrapped with the --mrs-development <user> option the Router also serves REST services marked \"in development\" with this column\'s \"$.developers\" including the same name as the <user> specified during bootstrap, while these REST services marked \"in development\" take priority over services with the same url_host+url_context_root context path and this column being NULL.',
  `comments` VARCHAR(512) NULL,
  `options` JSON NULL,
  `auth_path` VARCHAR(255) NOT NULL DEFAULT '/authentication' COMMENT 'The path used for authentication. The following sub-paths will be made available for <service_path>/<auth_path>:  /login /status /logout /completed',
  `auth_completed_url` VARCHAR(255) NULL COMMENT 'The authentication workflow will redirect to this URL after successful- or failed login. If this field is not set, the workflow will redirect to <service_path>/<auth_path>/completed if the <service_path>/<auth_path>/login?onCompletionRedirect parameter has not been set.',
  `auth_completed_url_validation` VARCHAR(512) NULL COMMENT 'A regular expression to validate the <service_path>/<auth_path>/login?onCompletionRedirect parameter. If set, this allows to limit the possible URLs an application can specify for this parameter.',
  `auth_completed_page_content` TEXT NULL COMMENT 'If this field is set its content will replace the page content of the /completed page.',
  `enable_sql_endpoint` TINYINT NOT NULL DEFAULT 0,
  `custom_metadata_schema` VARCHAR(255) NULL,
  `metadata` JSON NULL,
  PRIMARY KEY (`id`),
  INDEX `fk_service_url_host1_idx` (`url_host_id` ASC) VISIBLE,
  INDEX `fk_service_service1_idx` (`parent_id` ASC) VISIBLE,
  CONSTRAINT `fk_service_url_host1`
    FOREIGN KEY (`url_host_id`)
    REFERENCES `mysql_rest_service_metadata`.`url_host` (`id`)
    ON DELETE RESTRICT
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_service_service1`
    FOREIGN KEY (`parent_id`)
    REFERENCES `mysql_rest_service_metadata`.`service` (`id`)
    ON DELETE RESTRICT
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`db_schema`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`db_schema` (
  `id` BINARY(16) NOT NULL,
  `service_id` BINARY(16) NOT NULL,
  `name` VARCHAR(255) NOT NULL,
  `schema_type` ENUM('DATABASE_SCHEMA', 'SCRIPT_MODULE') NOT NULL DEFAULT 'DATABASE_SCHEMA',
  `request_path` VARCHAR(255) NOT NULL,
  `requires_auth` TINYINT NOT NULL DEFAULT 0,
  `enabled` TINYINT NOT NULL DEFAULT 1,
  `internal` TINYINT NOT NULL DEFAULT 0,
  `items_per_page` INT UNSIGNED NULL DEFAULT 25,
  `comments` VARCHAR(512) NULL,
  `options` JSON NULL,
  `metadata` JSON NULL,
  PRIMARY KEY (`id`),
  INDEX `fk_db_schema_service1_idx` (`service_id` ASC) VISIBLE,
  CONSTRAINT `fk_db_schema_service1`
    FOREIGN KEY (`service_id`)
    REFERENCES `mysql_rest_service_metadata`.`service` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`db_object`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`db_object` (
  `id` BINARY(16) NOT NULL,
  `db_schema_id` BINARY(16) NOT NULL,
  `name` VARCHAR(255) NOT NULL,
  `request_path` VARCHAR(255) NOT NULL,
  `enabled` TINYINT NOT NULL DEFAULT 1,
  `internal` TINYINT NOT NULL DEFAULT 0,
  `object_type` ENUM('TABLE', 'VIEW', 'PROCEDURE', 'FUNCTION', 'SCRIPT') NOT NULL,
  `crud_operations` SET('CREATE', 'READ', 'UPDATE', 'DELETE') NOT NULL DEFAULT '' COMMENT 'Calculated by the duality view options of object and object_reference table, always UPDATE for procedures and functions.',
  `format` ENUM('FEED', 'ITEM', 'MEDIA') NOT NULL DEFAULT 'FEED' COMMENT 'The HTTP request method for this handler. \'feed\' executes the source query and returns the result set in JSON representation, \'item\' returns a single row instead, \'media\' turns the result set into a binary representation with accompanying HTTP Content-Type header.',
  `items_per_page` INT UNSIGNED NULL,
  `media_type` VARCHAR(45) NULL,
  `auto_detect_media_type` TINYINT NOT NULL DEFAULT 0,
  `requires_auth` TINYINT NOT NULL DEFAULT 0,
  `auth_stored_procedure` VARCHAR(255) NULL DEFAULT 0 COMMENT 'Specifies the STORE PROCEDURE that should be called to identify if the given user is allowed to perform the given CRUD operation. The SP has to be in the same schema as the schema object and it has to accept the following parameters: (user_id, schema, object, crud_operation).  It returns true or false.',
  `options` JSON NULL COMMENT 'Holds additional options for the db_object, e.g. {\"id_generation\": \"auto_increment\"}. \"id_generation\" can be undefined or \"auto_increment\" for tables using AUTO_INCREMENT or \"reverse_uuid\" for tables using DECIMAL(16) for the primary key.',
  `details` JSON NULL,
  `comments` VARCHAR(512) NULL,
  `metadata` JSON NULL,
  PRIMARY KEY (`id`),
  INDEX `fk_db_objects_db_schema1_idx` (`db_schema_id` ASC) INVISIBLE,
  CONSTRAINT `fk_db_objects_db_schema1`
    FOREIGN KEY (`db_schema_id`)
    REFERENCES `mysql_rest_service_metadata`.`db_schema` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`auth_vendor`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`auth_vendor` (
  `id` BINARY(16) NOT NULL,
  `name` VARCHAR(65) NOT NULL,
  `validation_url` VARCHAR(255) NULL COMMENT 'URL used to validate the access_token provided by the client. Example: https://graph.facebook.com/debug_token?input_token=%access_token%&access_token=%app_access_token%',
  `enabled` TINYINT NOT NULL DEFAULT 1,
  `comments` VARCHAR(512) NULL,
  `options` JSON NULL,
  PRIMARY KEY (`id`))
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`auth_app`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`auth_app` (
  `id` BINARY(16) NOT NULL,
  `auth_vendor_id` BINARY(16) NOT NULL,
  `name` VARCHAR(45) NULL,
  `description` VARCHAR(512) NULL,
  `url` VARCHAR(255) NULL,
  `url_direct_auth` VARCHAR(255) NULL,
  `access_token` VARCHAR(1024) NULL COMMENT 'The app access token to validate the user login.',
  `app_id` VARCHAR(1024) NULL,
  `enabled` TINYINT NULL,
  `limit_to_registered_users` TINYINT NOT NULL DEFAULT 1 COMMENT 'Limit the users that can log in to the list of users in the auth_user table. The auth_user table can be pre-filled with users by specifying the name and email only. The vendor_user_id will be added on the first login automatically.',
  `default_role_id` BINARY(16) NULL COMMENT 'If set, a new user that has not any auth_roles assigned will get this role assigned when he logs in the first time.',
  `options` JSON NULL,
  PRIMARY KEY (`id`),
  INDEX `fk_auth_app_auth_vendor1_idx` (`auth_vendor_id` ASC) VISIBLE,
  CONSTRAINT `fk_auth_app_auth_vendor1`
    FOREIGN KEY (`auth_vendor_id`)
    REFERENCES `mysql_rest_service_metadata`.`auth_vendor` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`mrs_user`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`mrs_user` (
  `id` BINARY(16) NOT NULL,
  `auth_app_id` BINARY(16) NOT NULL,
  `name` VARCHAR(225) NULL,
  `email` VARCHAR(255) NULL,
  `vendor_user_id` VARCHAR(255) NULL,
  `login_permitted` TINYINT NOT NULL DEFAULT 0,
  `mapped_user_id` VARCHAR(255) NULL,
  `app_options` JSON NULL,
  `auth_string` TEXT NULL,
  `options` JSON NULL,
  PRIMARY KEY (`id`),
  INDEX `fk_auth_user_auth_app1_idx` (`auth_app_id` ASC) VISIBLE,
  CONSTRAINT `fk_auth_user_auth_app1`
    FOREIGN KEY (`auth_app_id`)
    REFERENCES `mysql_rest_service_metadata`.`auth_app` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`config`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`config` (
  `id` TINYINT NOT NULL DEFAULT 1,
  `service_enabled` TINYINT NULL,
  `data` JSON NULL,
  PRIMARY KEY (`id`))
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`redirect`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`redirect` (
  `id` BINARY(16) NOT NULL,
  `pattern` VARCHAR(1024) NULL,
  `target` VARCHAR(512) NULL,
  PRIMARY KEY (`id`))
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`url_host_alias`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`url_host_alias` (
  `id` BINARY(16) NOT NULL,
  `url_host_id` BINARY(16) NOT NULL,
  `alias` VARCHAR(255) NOT NULL COMMENT 'Specifies additional aliases for the given host, e.g. www.example.com',
  PRIMARY KEY (`id`),
  INDEX `fk_url_host_alias_url_host1_idx` (`url_host_id` ASC) VISIBLE,
  CONSTRAINT `fk_url_host_alias_url_host1`
    FOREIGN KEY (`url_host_id`)
    REFERENCES `mysql_rest_service_metadata`.`url_host` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`content_set`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`content_set` (
  `id` BINARY(16) NOT NULL,
  `service_id` BINARY(16) NOT NULL,
  `content_type` ENUM('STATIC', 'SCRIPTS') NOT NULL DEFAULT 'STATIC',
  `request_path` VARCHAR(255) NOT NULL,
  `requires_auth` TINYINT NOT NULL DEFAULT 0,
  `enabled` TINYINT NOT NULL DEFAULT 0,
  `internal` TINYINT NOT NULL DEFAULT 0,
  `comments` VARCHAR(512) NULL,
  `options` JSON NULL,
  PRIMARY KEY (`id`),
  INDEX `fk_static_content_version_service1_idx` (`service_id` ASC) VISIBLE,
  CONSTRAINT `fk_static_content_version_service1`
    FOREIGN KEY (`service_id`)
    REFERENCES `mysql_rest_service_metadata`.`service` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`content_file`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`content_file` (
  `id` BINARY(16) NOT NULL,
  `content_set_id` BINARY(16) NOT NULL,
  `request_path` VARCHAR(255) NOT NULL DEFAULT '/',
  `requires_auth` TINYINT NOT NULL DEFAULT 0,
  `enabled` TINYINT NOT NULL DEFAULT 1,
  `content` LONGBLOB NOT NULL,
  `size` BIGINT GENERATED ALWAYS AS (LENGTH(content)) STORED,
  `options` JSON NULL,
  PRIMARY KEY (`id`),
  INDEX `fk_content_content_set1_idx` (`content_set_id` ASC) VISIBLE,
  CONSTRAINT `fk_content_content_set1`
    FOREIGN KEY (`content_set_id`)
    REFERENCES `mysql_rest_service_metadata`.`content_set` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`audit_log`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`audit_log` (
  `id` INT NOT NULL AUTO_INCREMENT,
  `schema_name` VARCHAR(255) NULL,
  `table_name` VARCHAR(255) NOT NULL,
  `dml_type` ENUM('INSERT', 'UPDATE', 'DELETE') NOT NULL,
  `old_row_data` JSON NULL,
  `new_row_data` JSON NULL,
  `changed_by` VARCHAR(255) NOT NULL,
  `changed_at` TIMESTAMP NOT NULL,
  `old_row_id` BINARY(16) NULL,
  `new_row_id` BINARY(16) NULL,
  PRIMARY KEY (`id`),
  INDEX `idx_table_name` (`table_name` ASC) VISIBLE,
  INDEX `idx_changed_at` (`changed_at` ASC) VISIBLE,
  INDEX `idx_changed_by` (`changed_by` ASC) VISIBLE,
  INDEX `idx_new_row_id` (`new_row_id` ASC) VISIBLE,
  INDEX `idx_old_row_id` (`old_row_id` ASC) VISIBLE)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`mrs_role`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`mrs_role` (
  `id` BINARY(16) NOT NULL,
  `derived_from_role_id` BINARY(16) NULL,
  `specific_to_service_id` BINARY(16) NULL,
  `caption` VARCHAR(150) NOT NULL,
  `description` VARCHAR(512) NULL,
  `options` JSON NULL,
  PRIMARY KEY (`id`),
  INDEX `fk_priv_role_priv_role1_idx` (`derived_from_role_id` ASC) VISIBLE,
  INDEX `fk_auth_role_service1_idx` (`specific_to_service_id` ASC) VISIBLE,
  UNIQUE INDEX `auth_role_unique_caption` (`caption` ASC) VISIBLE,
  CONSTRAINT `fk_priv_role_priv_role1`
    FOREIGN KEY (`derived_from_role_id`)
    REFERENCES `mysql_rest_service_metadata`.`mrs_role` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_auth_role_service1`
    FOREIGN KEY (`specific_to_service_id`)
    REFERENCES `mysql_rest_service_metadata`.`service` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`mrs_user_has_role`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`mrs_user_has_role` (
  `user_id` BINARY(16) NOT NULL,
  `role_id` BINARY(16) NOT NULL,
  `comments` VARCHAR(512) NULL,
  `options` JSON NULL,
  PRIMARY KEY (`user_id`, `role_id`),
  INDEX `fk_auth_user_has_privilege_role_privilege_role1_idx` (`role_id` ASC) VISIBLE,
  INDEX `fk_auth_user_has_privilege_role_auth_user1_idx` (`user_id` ASC) VISIBLE,
  CONSTRAINT `fk_auth_user_has_privilege_role_auth_user1`
    FOREIGN KEY (`user_id`)
    REFERENCES `mysql_rest_service_metadata`.`mrs_user` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_auth_user_has_privilege_role_privilege_role1`
    FOREIGN KEY (`role_id`)
    REFERENCES `mysql_rest_service_metadata`.`mrs_role` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`mrs_user_hierarchy_type`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`mrs_user_hierarchy_type` (
  `id` BINARY(16) NOT NULL,
  `caption` VARCHAR(150) NULL,
  `description` VARCHAR(512) NULL,
  `specific_to_service_id` BINARY(16) NULL,
  `options` JSON NULL,
  PRIMARY KEY (`id`),
  INDEX `fk_user_hierarchy_type_service1_idx` (`specific_to_service_id` ASC) VISIBLE,
  CONSTRAINT `fk_user_hierarchy_type_service1`
    FOREIGN KEY (`specific_to_service_id`)
    REFERENCES `mysql_rest_service_metadata`.`service` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`mrs_user_hierarchy`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`mrs_user_hierarchy` (
  `user_id` BINARY(16) NOT NULL,
  `reporting_to_user_id` BINARY(16) NOT NULL,
  `user_hierarchy_type_id` BINARY(16) NOT NULL,
  `options` JSON NULL,
  PRIMARY KEY (`user_id`, `reporting_to_user_id`, `user_hierarchy_type_id`),
  INDEX `fk_user_hierarchy_auth_user2_idx` (`reporting_to_user_id` ASC) VISIBLE,
  INDEX `fk_user_hierarchy_hierarchy_type1_idx` (`user_hierarchy_type_id` ASC) VISIBLE,
  CONSTRAINT `fk_user_hierarchy_auth_user1`
    FOREIGN KEY (`user_id`)
    REFERENCES `mysql_rest_service_metadata`.`mrs_user` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_user_hierarchy_auth_user2`
    FOREIGN KEY (`reporting_to_user_id`)
    REFERENCES `mysql_rest_service_metadata`.`mrs_user` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_user_hierarchy_hierarchy_type1`
    FOREIGN KEY (`user_hierarchy_type_id`)
    REFERENCES `mysql_rest_service_metadata`.`mrs_user_hierarchy_type` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`mrs_privilege`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`mrs_privilege` (
  `id` BINARY(16) NOT NULL,
  `role_id` BINARY(16) NOT NULL,
  `crud_operations` SET('CREATE', 'READ', 'UPDATE', 'DELETE') NOT NULL DEFAULT '',
  `service_id` BINARY(16) NULL,
  `db_schema_id` BINARY(16) NULL,
  `db_object_id` BINARY(16) NULL,
  `service_path` VARCHAR(255) NULL,
  `schema_path` VARCHAR(255) NULL,
  `object_path` VARCHAR(255) NULL,
  `options` JSON NULL,
  PRIMARY KEY (`id`),
  INDEX `fk_priv_on_schema_db_schema1_idx` (`db_schema_id` ASC) VISIBLE,
  INDEX `fk_priv_on_schema_service1_idx` (`service_id` ASC) VISIBLE,
  INDEX `fk_priv_on_schema_db_object1_idx` (`db_object_id` ASC) VISIBLE,
  CONSTRAINT `fk_priv_on_schema_auth_role1`
    FOREIGN KEY (`role_id`)
    REFERENCES `mysql_rest_service_metadata`.`mrs_role` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_priv_on_schema_db_schema1`
    FOREIGN KEY (`db_schema_id`)
    REFERENCES `mysql_rest_service_metadata`.`db_schema` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_priv_on_schema_service1`
    FOREIGN KEY (`service_id`)
    REFERENCES `mysql_rest_service_metadata`.`service` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_priv_on_schema_db_object1`
    FOREIGN KEY (`db_object_id`)
    REFERENCES `mysql_rest_service_metadata`.`db_object` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`mrs_user_group`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`mrs_user_group` (
  `id` BINARY(16) NOT NULL,
  `specific_to_service_id` BINARY(16) NULL,
  `caption` VARCHAR(45) NULL,
  `description` VARCHAR(512) NULL,
  `options` JSON NULL,
  PRIMARY KEY (`id`),
  INDEX `fk_user_group_service1_idx` (`specific_to_service_id` ASC) VISIBLE,
  CONSTRAINT `fk_user_group_service1`
    FOREIGN KEY (`specific_to_service_id`)
    REFERENCES `mysql_rest_service_metadata`.`service` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`mrs_user_group_has_role`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`mrs_user_group_has_role` (
  `user_group_id` BINARY(16) NOT NULL,
  `role_id` BINARY(16) NOT NULL,
  `options` JSON NULL,
  PRIMARY KEY (`user_group_id`, `role_id`),
  INDEX `fk_user_group_has_auth_role_auth_role1_idx` (`role_id` ASC) VISIBLE,
  INDEX `fk_user_group_has_auth_role_user_group1_idx` (`user_group_id` ASC) VISIBLE,
  CONSTRAINT `fk_user_group_has_auth_role_user_group1`
    FOREIGN KEY (`user_group_id`)
    REFERENCES `mysql_rest_service_metadata`.`mrs_user_group` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_user_group_has_auth_role_auth_role1`
    FOREIGN KEY (`role_id`)
    REFERENCES `mysql_rest_service_metadata`.`mrs_role` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`mrs_user_has_group`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`mrs_user_has_group` (
  `user_id` BINARY(16) NOT NULL,
  `user_group_id` BINARY(16) NOT NULL,
  `comments` VARCHAR(512) NULL,
  `options` JSON NULL,
  PRIMARY KEY (`user_id`, `user_group_id`),
  INDEX `fk_auth_user_has_user_group_user_group1_idx` (`user_group_id` ASC) VISIBLE,
  INDEX `fk_auth_user_has_user_group_auth_user1_idx` (`user_id` ASC) VISIBLE,
  CONSTRAINT `fk_auth_user_has_user_group_auth_user1`
    FOREIGN KEY (`user_id`)
    REFERENCES `mysql_rest_service_metadata`.`mrs_user` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_auth_user_has_user_group_user_group1`
    FOREIGN KEY (`user_group_id`)
    REFERENCES `mysql_rest_service_metadata`.`mrs_user_group` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`mrs_group_hierarchy_type`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`mrs_group_hierarchy_type` (
  `id` BINARY(16) NOT NULL,
  `caption` VARCHAR(150) NULL,
  `description` VARCHAR(512) NULL,
  `options` JSON NULL,
  PRIMARY KEY (`id`))
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`mrs_user_group_hierarchy`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`mrs_user_group_hierarchy` (
  `user_group_id` BINARY(16) NOT NULL,
  `parent_group_id` BINARY(16) NOT NULL,
  `group_hierarchy_type_id` BINARY(16) NOT NULL,
  `level` INT UNSIGNED NOT NULL DEFAULT 0,
  `options` JSON NULL,
  PRIMARY KEY (`user_group_id`, `parent_group_id`, `group_hierarchy_type_id`),
  INDEX `fk_user_group_has_user_group_user_group2_idx` (`parent_group_id` ASC) VISIBLE,
  INDEX `fk_user_group_has_user_group_user_group1_idx` (`user_group_id` ASC) VISIBLE,
  INDEX `fk_user_group_hierarchy_group_hierarchy_type1_idx` (`group_hierarchy_type_id` ASC) VISIBLE,
  CONSTRAINT `fk_user_group_has_user_group_user_group1`
    FOREIGN KEY (`user_group_id`)
    REFERENCES `mysql_rest_service_metadata`.`mrs_user_group` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_user_group_has_user_group_user_group2`
    FOREIGN KEY (`parent_group_id`)
    REFERENCES `mysql_rest_service_metadata`.`mrs_user_group` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_user_group_hierarchy_group_hierarchy_type1`
    FOREIGN KEY (`group_hierarchy_type_id`)
    REFERENCES `mysql_rest_service_metadata`.`mrs_group_hierarchy_type` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`mrs_db_object_row_group_security`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`mrs_db_object_row_group_security` (
  `db_object_id` BINARY(16) NOT NULL,
  `group_hierarchy_type_id` BINARY(16) NOT NULL,
  `row_group_ownership_column` VARCHAR(255) NOT NULL,
  `level` INT UNSIGNED NOT NULL DEFAULT 0,
  `match_level` ENUM('HIGHER', 'EQUAL OR HIGHER', 'EQUAL', 'LOWER OR EQUAL', 'LOWER') NOT NULL DEFAULT 'HIGHER',
  `options` JSON NULL,
  INDEX `fk_table1_db_object1_idx` (`db_object_id` ASC) VISIBLE,
  INDEX `fk_db_object_row_security_group_hierarchy_type1_idx` (`group_hierarchy_type_id` ASC) VISIBLE,
  PRIMARY KEY (`db_object_id`, `group_hierarchy_type_id`),
  CONSTRAINT `fk_table1_db_object1`
    FOREIGN KEY (`db_object_id`)
    REFERENCES `mysql_rest_service_metadata`.`db_object` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_db_object_row_security_group_hierarchy_type1`
    FOREIGN KEY (`group_hierarchy_type_id`)
    REFERENCES `mysql_rest_service_metadata`.`mrs_group_hierarchy_type` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`router`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`router` (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'The ID of the router instance that uniquely identifies the router on this MySQL REST Service setup.',
  `router_name` VARCHAR(255) NOT NULL COMMENT 'A user specified name for an instance of the router. Should default to address:port, where port is the RW port for classic protocol. Set via --name during router bootstrap.',
  `address` VARCHAR(255) CHARACTER SET 'ascii' COLLATE 'ascii_general_ci' NOT NULL COMMENT 'Network address of the host the Router is running on. Set via --report--host during bootstrap.',
  `product_name` VARCHAR(128) NOT NULL COMMENT 'The product name of the routing component, e.g. \'MySQL Router\'',
  `version` VARCHAR(12) NULL COMMENT 'The version of the router instance. Updated on bootstrap and each startup of the router instance. Format: x.y.z, 3 digits for each component. Managed by Router.',
  `last_check_in` TIMESTAMP NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT 'A timestamp updated by the router every hour with the current time. This timestamp is used to detect routers that are no longer used or stalled. Managed by Router.',
  `attributes` JSON NULL COMMENT 'Router specific custom attributes. Managed by Router.',
  `options` JSON NULL COMMENT 'Router instance specific configuration options.',
  PRIMARY KEY (`id`),
  UNIQUE INDEX `address_router_name` (`address` ASC, `router_name` ASC) VISIBLE)
ENGINE = InnoDB
COMMENT = 'no_audit_log';


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`router_status`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`router_status` (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `router_id` INT UNSIGNED NOT NULL,
  `status_time` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT 'The time the status was reported',
  `timespan` SMALLINT NOT NULL COMMENT 'The timespan of the measuring interval',
  `mysql_connections` MEDIUMINT NOT NULL DEFAULT 0,
  `mysql_queries` MEDIUMINT NOT NULL DEFAULT 0,
  `http_requests_get` MEDIUMINT NOT NULL DEFAULT 0,
  `http_requests_post` MEDIUMINT NOT NULL DEFAULT 0,
  `http_requests_put` MEDIUMINT NOT NULL DEFAULT 0,
  `http_requests_delete` MEDIUMINT NOT NULL DEFAULT 0,
  `active_mysql_connections` MEDIUMINT NOT NULL DEFAULT 0,
  `details` JSON NULL COMMENT 'More detailed status information',
  PRIMARY KEY (`id`),
  INDEX `fk_router_status_router1_idx` (`router_id` ASC) VISIBLE,
  INDEX `status_time` (`status_time` ASC) VISIBLE,
  CONSTRAINT `fk_router_status_router1`
    FOREIGN KEY (`router_id`)
    REFERENCES `mysql_rest_service_metadata`.`router` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB
COMMENT = 'no_audit_log';


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`router_session`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`router_session` (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `user_id` BINARY(16) NOT NULL,
  `service_id` BINARY(16) NOT NULL,
  `expires` DATETIME NOT NULL,
  PRIMARY KEY (`id`))
ENGINE = InnoDB
COMMENT = 'no_audit_log';


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`router_general_log`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`router_general_log` (
  `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
  `router_id` INT UNSIGNED NOT NULL,
  `router_session_id` INT UNSIGNED NULL,
  `log_time` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `log_type` ENUM("INFO", "WARNING", "ERROR") NOT NULL,
  `code` SMALLINT UNSIGNED NULL,
  `message` VARCHAR(255) NULL,
  `data` JSON NULL,
  PRIMARY KEY (`id`),
  INDEX `fk_router_general_log_router1_idx` (`router_id` ASC) VISIBLE,
  INDEX `log_time` (`log_time` ASC) VISIBLE,
  INDEX `fk_router_general_log_router_session1_idx` (`router_session_id` ASC) VISIBLE,
  CONSTRAINT `fk_router_general_log_router1`
    FOREIGN KEY (`router_id`)
    REFERENCES `mysql_rest_service_metadata`.`router` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_router_general_log_router_session1`
    FOREIGN KEY (`router_session_id`)
    REFERENCES `mysql_rest_service_metadata`.`router_session` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB
COMMENT = 'no_audit_log';


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`object`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`object` (
  `id` BINARY(16) NOT NULL,
  `db_object_id` BINARY(16) NOT NULL,
  `name` VARCHAR(255) NOT NULL,
  `kind` ENUM('RESULT', 'PARAMETERS', 'INTERFACE') NOT NULL DEFAULT 'RESULT',
  `position` INT NOT NULL DEFAULT 0,
  `row_ownership_field_id` BINARY(16) NULL,
  `options` JSON NULL COMMENT 'Holds duality view options for INSERT, UPDATE, DELETE and CHECK, e.g. { duality_view_insert: true, duality_view_update: true, duality_view_delete: false, duality_view_no_check: false }',
  `sdk_options` JSON NULL,
  `comments` VARCHAR(512) NULL,
  PRIMARY KEY (`id`),
  INDEX `fk_result_db_object1_idx` (`db_object_id` ASC) VISIBLE,
  INDEX `row_ownership_object_idx` (`row_ownership_field_id` ASC) VISIBLE,
  CONSTRAINT `fk_result_db_object1`
    FOREIGN KEY (`db_object_id`)
    REFERENCES `mysql_rest_service_metadata`.`db_object` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`object_reference`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`object_reference` (
  `id` BINARY(16) NOT NULL,
  `reduce_to_value_of_field_id` BINARY(16) NULL COMMENT 'If set to an object_field, this reference will be reduced to the value of the given field. Example: \"films\": [ { \"categories\": [ \"Thriller\", \"Action\"] } ] instead of \"films\": [ { \"categories\": [ { \"name\": \"Thriller\" }, { \"name\": \"Action\" } ] } ],',
  `row_ownership_field_id` BINARY(16) NULL,
  `reference_mapping` JSON NOT NULL COMMENT 'Holds all column mappings of the FK, {kind:\"n:1\", constraint: \"constraint_name\", referenced_schema: \"schema_name\", referenced_table: \"table_name\", column_mapping: [{\"column_name\": \"referenced_column_name\"}, \"to_many\": true, \"id_generation\": \"auto_increment\"}. \"id_generation\" can be undefined or \"auto_increment\" for tables using AUTO_INCREMENT or \"reverse_uuid\" for tables using BINARY(16) for the primary key.',
  `unnest` BIT(1) NOT NULL DEFAULT 0 COMMENT 'If set to TRUE, the properties will be directly added to the parent',
  `options` JSON NULL COMMENT 'Holds duality view options for INSERT, UPDATE, DELETE and CHECK, e.g. { duality_view_insert: true, duality_view_update: true, duality_view_delete: false, duality_view_no_check: false }',
  `sdk_options` JSON NULL,
  `comments` VARCHAR(512) NULL,
  PRIMARY KEY (`id`),
  INDEX `reduce_to_idx` (`reduce_to_value_of_field_id` ASC) VISIBLE,
  INDEX `row_ownership_object_reference_idx` (`row_ownership_field_id` ASC) VISIBLE)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`object_field`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`object_field` (
  `id` BINARY(16) NOT NULL,
  `object_id` BINARY(16) NOT NULL,
  `parent_reference_id` BINARY(16) NULL,
  `represents_reference_id` BINARY(16) NULL,
  `name` VARCHAR(255) NOT NULL COMMENT 'The name of the field as returned in the JSON',
  `position` INT NOT NULL,
  `db_column` JSON NULL COMMENT 'Holds information about the original database column, e.g. {\"name\": \"first_name\", \"datatype\":\"VARCHAR(45)\", \"not_null\": true, \"is_primary\": false, \"is_unique\": false, \"is_generated\": false, \"auto_inc\": false}. When representing a STORED PROCEDURE parameter, two optional fields can be set, {\"in\": true, \"out\": false}',
  `enabled` BIT(1) NOT NULL DEFAULT 1 COMMENT 'When set to FALSE, the property is hidden from the result',
  `allow_filtering` BIT(1) NOT NULL DEFAULT 1 COMMENT 'When set to FALSE the property is not available for filtering',
  `allow_sorting` BIT(1) NOT NULL DEFAULT 0 COMMENT 'When set to TRUE the field can be used for ordering',
  `no_check` BIT(1) NOT NULL DEFAULT 0 COMMENT 'Specifies whether the field should be ignored in the scope of concurrency control',
  `no_update` BIT(1) NOT NULL DEFAULT 0 COMMENT 'If set to 1 then no updates of this field are allowed.',
  `options` JSON NULL,
  `sdk_options` JSON NULL,
  `comments` VARCHAR(512) NULL,
  PRIMARY KEY (`id`),
  INDEX `fk_properties_result1_idx` (`object_id` ASC) VISIBLE,
  INDEX `fk_result_property_result_reference1_idx` (`parent_reference_id` ASC) VISIBLE,
  INDEX `fk_result_property_result_reference2_idx` (`represents_reference_id` ASC) VISIBLE,
  CONSTRAINT `fk_properties_result1`
    FOREIGN KEY (`object_id`)
    REFERENCES `mysql_rest_service_metadata`.`object` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_result_property_result_reference1`
    FOREIGN KEY (`parent_reference_id`)
    REFERENCES `mysql_rest_service_metadata`.`object_reference` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_result_property_result_reference2`
    FOREIGN KEY (`represents_reference_id`)
    REFERENCES `mysql_rest_service_metadata`.`object_reference` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`service_has_auth_app`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`service_has_auth_app` (
  `service_id` BINARY(16) NOT NULL,
  `auth_app_id` BINARY(16) NOT NULL,
  `options` JSON NULL,
  PRIMARY KEY (`service_id`, `auth_app_id`),
  INDEX `fk_service_has_auth_app_auth_app1_idx` (`auth_app_id` ASC) VISIBLE,
  INDEX `fk_service_has_auth_app_service1_idx` (`service_id` ASC) VISIBLE,
  CONSTRAINT `fk_service_has_auth_app_service1`
    FOREIGN KEY (`service_id`)
    REFERENCES `mysql_rest_service_metadata`.`service` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_service_has_auth_app_auth_app1`
    FOREIGN KEY (`auth_app_id`)
    REFERENCES `mysql_rest_service_metadata`.`auth_app` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;


# -----------------------------------------------------
# Table `mysql_rest_service_metadata`.`content_set_has_obj_def`
# -----------------------------------------------------
CREATE TABLE IF NOT EXISTS `mysql_rest_service_metadata`.`content_set_has_obj_def` (
  `content_set_id` BINARY(16) NOT NULL,
  `db_object_id` BINARY(16) NOT NULL,
  `kind` ENUM('Script', 'BeforeCreate', 'BeforeRead', 'BeforeUpdate', 'BeforeDelete', 'AfterCreate', 'AfterRead', 'AfterUpdate', 'AfterDelete') NOT NULL,
  `priority` INT NOT NULL DEFAULT 0,
  `language` VARCHAR(45) NOT NULL,
  `name` VARCHAR(255) NOT NULL,
  `class_name` VARCHAR(255) NULL,
  `comments` VARCHAR(512) NULL,
  `options` JSON NULL,
  INDEX `fk_content_set_has_obj_dev_db_object1_idx` (`db_object_id` ASC) VISIBLE,
  INDEX `fk_content_set_has_obj_dev_content_set1_idx` (`content_set_id` ASC) VISIBLE,
  INDEX `content_set_has_obj_dev_priority` (`priority` ASC) VISIBLE,
  PRIMARY KEY (`content_set_id`, `db_object_id`, `kind`, `priority`),
  INDEX `content_set_has_obj_dev_method_type` (`kind` ASC) VISIBLE,
  CONSTRAINT `fk_content_set_has_db_object_content_set1`
    FOREIGN KEY (`content_set_id`)
    REFERENCES `mysql_rest_service_metadata`.`content_set` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION,
  CONSTRAINT `fk_content_set_has_db_object_db_object1`
    FOREIGN KEY (`db_object_id`)
    REFERENCES `mysql_rest_service_metadata`.`db_object` (`id`)
    ON DELETE NO ACTION
    ON UPDATE NO ACTION)
ENGINE = InnoDB;

USE `mysql_rest_service_metadata` ;

# -----------------------------------------------------
# View `mysql_rest_service_metadata`.`mrs_user_schema_version`
# -----------------------------------------------------
USE `mysql_rest_service_metadata`;
CREATE  OR REPLACE SQL SECURITY INVOKER VIEW mrs_user_schema_version (major, minor, patch) AS SELECT 3, 0, 2;

# -----------------------------------------------------
# View `mysql_rest_service_metadata`.`object_fields_with_references`
# -----------------------------------------------------
USE `mysql_rest_service_metadata`;
CREATE  OR REPLACE SQL SECURITY INVOKER VIEW `object_fields_with_references` AS
WITH RECURSIVE obj_fields (
    caption, lev, position, id, represents_reference_id, parent_reference_id, object_id,
    name, db_column, enabled,
    allow_filtering, allow_sorting, no_check, no_update, options, sdk_options, comments,
    object_reference) AS
(
    SELECT CONCAT("- ", f.name) as caption, 1 AS lev, f.position, f.id,
        f.represents_reference_id, f.parent_reference_id, f.object_id, f.name,
        f.db_column, f.enabled, f.allow_filtering, f.allow_sorting, f.no_check, f.no_update,
        f.options, f.sdk_options, f.comments,
        IF(ISNULL(f.represents_reference_id), NULL, JSON_OBJECT(
            "reduce_to_value_of_field_id", TO_BASE64(r.reduce_to_value_of_field_id),
            "row_ownership_field_id", TO_BASE64(r.row_ownership_field_id),
            "reference_mapping", r.reference_mapping,
            "unnest", (r.unnest = 1),
            "options", r.options,
            "sdk_options", r.sdk_options,
            "comments", r.comments
        )) AS object_reference
    FROM `mysql_rest_service_metadata`.`object_field` f
        LEFT OUTER JOIN `mysql_rest_service_metadata`.`object_reference` AS r
            ON r.id = f.represents_reference_id
    WHERE ISNULL(parent_reference_id)
    UNION ALL
    SELECT CONCAT(REPEAT("  ", p.lev), "- ", f.name) as caption, p.lev+1 AS lev, f.position,
        f.id, f.represents_reference_id, f.parent_reference_id, f.object_id, f.name,
        f.db_column, f.enabled, f.allow_filtering, f.allow_sorting, f.no_check, f.no_update,
        f.options, f.sdk_options, f.comments,
        IF(ISNULL(f.represents_reference_id), NULL, JSON_OBJECT(
            "reduce_to_value_of_field_id", TO_BASE64(rc.reduce_to_value_of_field_id),
            "row_ownership_field_id", TO_BASE64(rc.row_ownership_field_id),
            "reference_mapping", rc.reference_mapping,
            "unnest", (rc.unnest = 1),
            "options", rc.options,
            "sdk_options", rc.sdk_options,
            "comments", rc.comments
        )) AS object_reference
    FROM obj_fields AS p JOIN `mysql_rest_service_metadata`.`object_reference` AS r
            ON r.id = p.represents_reference_id
        LEFT OUTER JOIN `mysql_rest_service_metadata`.`object_field` AS f
            ON r.id = f.parent_reference_id
        LEFT OUTER JOIN `mysql_rest_service_metadata`.`object_reference` AS rc
            ON rc.id = f.represents_reference_id
    WHERE f.id IS NOT NULL
)
SELECT * FROM obj_fields;

# -----------------------------------------------------
# View `mysql_rest_service_metadata`.`table_columns_with_references`
# -----------------------------------------------------
USE `mysql_rest_service_metadata`;
CREATE  OR REPLACE SQL SECURITY INVOKER VIEW `table_columns_with_references` AS
SELECT f.* FROM (
    # Get the table columns
    SELECT c.ORDINAL_POSITION AS position, c.COLUMN_NAME AS name,
        NULL AS ref_column_names,
        JSON_OBJECT(
            "name", c.COLUMN_NAME,
            "datatype", c.COLUMN_TYPE,
            "not_null", c.IS_NULLABLE = "NO",
            "is_primary", c.COLUMN_KEY = "PRI",
            "is_unique", c.COLUMN_KEY = "UNI",
            "is_generated", c.GENERATION_EXPRESSION <> "",
            "id_generation", IF(c.EXTRA = "auto_increment", "auto_inc",
                IF(c.COLUMN_KEY = "PRI" AND c.DATA_TYPE = "binary" AND c.CHARACTER_MAXIMUM_LENGTH = 16,
                    "rev_uuid", NULL)),
            "comment", c.COLUMN_COMMENT,
            "srid", c.SRS_ID,
            "column_default", c.COLUMN_DEFAULT
            ) AS db_column,
        NULL AS reference_mapping,
        c.TABLE_SCHEMA as table_schema, c.TABLE_NAME as table_name
    FROM INFORMATION_SCHEMA.COLUMNS AS c
        LEFT OUTER JOIN INFORMATION_SCHEMA.KEY_COLUMN_USAGE AS k
            ON c.TABLE_SCHEMA = k.TABLE_SCHEMA AND c.TABLE_NAME = k.TABLE_NAME
                AND c.COLUMN_NAME=k.COLUMN_NAME
                AND NOT ISNULL(k.POSITION_IN_UNIQUE_CONSTRAINT)
    # Union with the references that point from the table to other tables (n:1)
    UNION
    SELECT MAX(c.ORDINAL_POSITION) + 100 AS position, MAX(k.REFERENCED_TABLE_NAME) AS name,
        GROUP_CONCAT(c.COLUMN_NAME SEPARATOR ', ') AS ref_column_names,
        NULL AS db_column,
        JSON_MERGE_PRESERVE(
            JSON_OBJECT("kind", "n:1"),
            JSON_OBJECT("constraint",
                CONCAT(MAX(k.CONSTRAINT_SCHEMA), ".", MAX(k.CONSTRAINT_NAME))),
            JSON_OBJECT("to_many", FALSE),
            JSON_OBJECT("referenced_schema", MAX(k.REFERENCED_TABLE_SCHEMA)),
            JSON_OBJECT("referenced_table", MAX(k.REFERENCED_TABLE_NAME)),
            JSON_OBJECT("column_mapping",
                JSON_ARRAYAGG(JSON_OBJECT(
                    "base", c.COLUMN_NAME,
                    "ref", k.REFERENCED_COLUMN_NAME)))
        ) AS reference_mapping,
        MAX(c.TABLE_SCHEMA) AS table_schema, MAX(c.TABLE_NAME) AS table_name
    FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE AS k
        JOIN INFORMATION_SCHEMA.COLUMNS AS c
            ON c.TABLE_SCHEMA = k.TABLE_SCHEMA AND c.TABLE_NAME = k.TABLE_NAME
                AND c.COLUMN_NAME=k.COLUMN_NAME
    WHERE NOT ISNULL(k.REFERENCED_TABLE_NAME)
    GROUP BY k.CONSTRAINT_NAME, k.table_schema, k.table_name
    UNION
    # Union with the references that point from other tables to the table (1:1 and 1:n)
    SELECT MAX(c.ORDINAL_POSITION) + 1000 AS position,
        MAX(c.TABLE_NAME) AS name,
        GROUP_CONCAT(k.COLUMN_NAME SEPARATOR ', ') AS ref_column_names,
        NULL AS db_column,
        JSON_MERGE_PRESERVE(
            # If the PKs of the table and the referred table are exactly the same,
            # this is a 1:1 relationship, otherwise an 1:n
            JSON_OBJECT("kind", IF(JSON_CONTAINS(MAX(PK_TABLE.PK), MAX(PK_REF.PK)) = 1,
                "1:1", "1:n")),
            JSON_OBJECT("constraint",
                CONCAT(MAX(k.CONSTRAINT_SCHEMA), ".", MAX(k.CONSTRAINT_NAME))),
            JSON_OBJECT("to_many", JSON_CONTAINS(MAX(PK_TABLE.PK), MAX(PK_REF.PK)) = 0),
            JSON_OBJECT("referenced_schema", MAX(c.TABLE_SCHEMA)),
            JSON_OBJECT("referenced_table", MAX(c.TABLE_NAME)),
            JSON_OBJECT("column_mapping",
                JSON_ARRAYAGG(JSON_OBJECT(
                    "base", k.REFERENCED_COLUMN_NAME,
                    "ref", c.COLUMN_NAME)))
        ) AS reference_mapping,
        MAX(k.REFERENCED_TABLE_SCHEMA) AS table_schema,
        MAX(k.REFERENCED_TABLE_NAME) AS table_name
    FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE AS k
        JOIN INFORMATION_SCHEMA.COLUMNS AS c
            ON c.TABLE_SCHEMA = k.TABLE_SCHEMA AND c.TABLE_NAME = k.TABLE_NAME
                AND c.COLUMN_NAME=k.COLUMN_NAME
        # The PK columns of the table, e.g. ["test_fk.product.id"]
        JOIN (SELECT JSON_ARRAYAGG(CONCAT(c2.TABLE_SCHEMA, ".",
                    c2.TABLE_NAME, ".", c2.COLUMN_NAME)) AS PK,
                c2.TABLE_SCHEMA, c2.TABLE_NAME
                FROM INFORMATION_SCHEMA.COLUMNS AS c2
                WHERE c2.COLUMN_KEY = "PRI"
                GROUP BY c2.COLUMN_KEY, c2.TABLE_SCHEMA, c2.TABLE_NAME) AS PK_TABLE
            ON PK_TABLE.TABLE_SCHEMA = k.REFERENCED_TABLE_SCHEMA
                AND PK_TABLE.TABLE_NAME = k.REFERENCED_TABLE_NAME
        # The PK columns of the referenced table,
        # e.g. ["test_fk.product_part.id", "test_fk.product.id"]
        JOIN (SELECT JSON_ARRAYAGG(PK2.PK_COL) AS PK, PK2.TABLE_SCHEMA, PK2.TABLE_NAME
            FROM (SELECT IFNULL(
                CONCAT(MAX(k1.REFERENCED_TABLE_SCHEMA), ".",
                    MAX(k1.REFERENCED_TABLE_NAME), ".", MAX(k1.REFERENCED_COLUMN_NAME)),
                CONCAT(c1.TABLE_SCHEMA, ".", c1.TABLE_NAME, ".", c1.COLUMN_NAME)) AS PK_COL,
                c1.TABLE_SCHEMA AS TABLE_SCHEMA, c1.TABLE_NAME AS TABLE_NAME
                FROM INFORMATION_SCHEMA.COLUMNS AS c1
                    JOIN INFORMATION_SCHEMA.KEY_COLUMN_USAGE AS k1
                        ON k1.TABLE_SCHEMA = c1.TABLE_SCHEMA
                            AND k1.TABLE_NAME = c1.TABLE_NAME
                            AND k1.COLUMN_NAME = c1.COLUMN_NAME
                WHERE c1.COLUMN_KEY = "PRI"
                GROUP BY c1.COLUMN_NAME, c1.TABLE_SCHEMA, c1.TABLE_NAME) AS PK2
                GROUP BY PK2.TABLE_SCHEMA, PK2.TABLE_NAME) AS PK_REF
            ON PK_REF.TABLE_SCHEMA = k.TABLE_SCHEMA AND PK_REF.TABLE_NAME = k.TABLE_NAME
    GROUP BY k.CONSTRAINT_NAME, c.TABLE_SCHEMA, c.TABLE_NAME
    ) AS f
ORDER BY f.position;

# -----------------------------------------------------
# View `mysql_rest_service_metadata`.`router_services`
# -----------------------------------------------------
USE `mysql_rest_service_metadata`;
CREATE  OR REPLACE SQL SECURITY INVOKER VIEW `router_services` AS
SELECT r.id AS router_id, r.router_name, r.address, r.attributes->>'$.developer' AS router_developer,
    s.id as service_id, h.name AS service_url_host_name,
    s.url_context_root AS service_url_context_root,
    CONCAT(h.name, s.url_context_root) AS service_host_ctx,
    s.published, s.in_development,
    (SELECT GROUP_CONCAT(IF(item REGEXP '^[A-Za-z0-9_]+$', item, QUOTE(item)) ORDER BY item)
        FROM JSON_TABLE(
        s.in_development->>'$.developers', '$[*]' COLUMNS (item text path '$')
    ) AS jt) AS sorted_developers
FROM `mysql_rest_service_metadata`.`service` s
    LEFT JOIN `mysql_rest_service_metadata`.`url_host` h
        ON s.url_host_id = h.id
    JOIN `mysql_rest_service_metadata`.`router` r
WHERE
    (enabled = 1)
    AND (
    ((published = 1) AND (NOT EXISTS (select s2.id from `mysql_rest_service_metadata`.`service` s2 where s.url_host_id=s2.url_host_id AND s.url_context_root=s2.url_context_root
        AND JSON_OVERLAPS(r.attributes->'$.developer', s2.in_development->>'$.developers'))))
    OR
    ((published = 0) AND (s.id IN (select s2.id from `mysql_rest_service_metadata`.`service` s2 where s.url_host_id=s2.url_host_id AND s.url_context_root=s2.url_context_root
        AND JSON_OVERLAPS(r.attributes->'$.developer', s2.in_development->>'$.developers'))))
    OR
    ((published = 0) AND (r.options->'$.developer' IS NOT NULL
        OR r.attributes->'$.developer' IS NOT NULL) AND s.in_development IS NULL)
    );
USE `mysql_rest_service_metadata`;

DELIMITER $$;
USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`url_host_BEFORE_DELETE` BEFORE DELETE ON `url_host` FOR EACH ROW
BEGIN
    DELETE FROM `mysql_rest_service_metadata`.`url_host_alias` WHERE `url_host_id` = OLD.`id`;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`service_BEFORE_INSERT` BEFORE INSERT ON `service` FOR EACH ROW
BEGIN
    # Check if the full service request_path (including the optional developer setting) already exists
    IF NEW.enabled = TRUE THEN
        SET @host_name := (SELECT h.name FROM `mysql_rest_service_metadata`.url_host h WHERE h.id = NEW.url_host_id);
        SET @request_path := CONCAT(COALESCE(NEW.in_development->>'$.developers', ''), @host_name, NEW.url_context_root);
        SET @validPath := (SELECT `mysql_rest_service_metadata`.`valid_request_path`(@request_path));

        IF @validPath = 0 THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "The request_path is already used by another entity.";
        END IF;

        # Check if the same developer is already registered in the in_development->>'$.developers' of a service with the very same host_ctx
        SET @validDeveloperList := (SELECT MAX(COALESCE(
                JSON_OVERLAPS(s.in_development->>'$.developers', NEW.in_development->>'$.developers'), FALSE)) AS overlap
            FROM `mysql_rest_service_metadata`.`service` AS s JOIN
                `mysql_rest_service_metadata`.`url_host` AS h ON s.url_host_id = h.id JOIN
                `mysql_rest_service_metadata`.`url_host` AS h2 ON h2.id = NEW.url_host_id
            WHERE CONCAT(h.name, s.url_context_root) = CONCAT(h2.name, NEW.url_context_root) AND s.enabled = TRUE
            GROUP BY CONCAT(h.name, s.url_context_root));

        IF COALESCE(@validDeveloperList, FALSE) = TRUE THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "This developer is already registered for a REST service with the same host/url_context_root path.";
        END IF;
    END IF;

    IF NEW.in_development IS NOT NULL THEN
        SET NEW.published = 0;
    END IF;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`service_BEFORE_UPDATE` BEFORE UPDATE ON `service` FOR EACH ROW
BEGIN
    # Check if the full service request_path (including the optional developer setting) already exists,
    # but only when the service is enabled and either of those values was actually changed
    IF NEW.enabled = TRUE AND (COALESCE(NEW.in_development, '') <> COALESCE(OLD.in_development, '')
        OR NEW.url_host_id <> OLD.url_host_id OR NEW.url_context_root <> OLD.url_context_root) THEN

        SET @host_name := (SELECT h.name FROM `mysql_rest_service_metadata`.url_host h WHERE h.id = NEW.url_host_id);
        SET @request_path := CONCAT(COALESCE(NEW.in_development->>'$.developers', ''), @host_name, NEW.url_context_root);
        SET @validPath := (SELECT `mysql_rest_service_metadata`.`valid_request_path`(@request_path));

        IF @validPath = 0 THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "The request_path is already used by another entity.";
        END IF;

        # Check if the same developer is already registered in the in_development->>'$.developers' of a service with the very same host_ctx
        SET @validDeveloperList := (SELECT MAX(COALESCE(
                JSON_OVERLAPS(s.in_development->>'$.developers', NEW.in_development->>'$.developers'), FALSE)) AS overlap
            FROM `mysql_rest_service_metadata`.`service` AS s JOIN
                `mysql_rest_service_metadata`.`url_host` AS h ON s.url_host_id = h.id JOIN
                `mysql_rest_service_metadata`.`url_host` AS h2 ON h2.id = NEW.url_host_id
            WHERE CONCAT(h.name, s.url_context_root) = CONCAT(h2.name, NEW.url_context_root) AND s.enabled = TRUE
                AND s.id <> NEW.id
            GROUP BY CONCAT(h.name, s.url_context_root));

        IF COALESCE(@validDeveloperList, FALSE) = TRUE THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "This developer is already registered for a REST service with the same host/url_context_root path.";
        END IF;
    END IF;

    IF OLD.in_development IS NULL AND NEW.in_development IS NOT NULL AND NEW.published = 1 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "A REST service that is in development cannot be published. Please reset the development state first.";
    END IF;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`service_BEFORE_DELETE` BEFORE DELETE ON `service` FOR EACH ROW
BEGIN
    # Since FKs do not fire the triggers on the related tables, manually trigger the DELETEs
    DELETE FROM `mysql_rest_service_metadata`.`db_schema` WHERE `service_id` = OLD.`id`;
    DELETE FROM `mysql_rest_service_metadata`.`content_set` WHERE `service_id` = OLD.`id`;
    DELETE FROM `mysql_rest_service_metadata`.`service_has_auth_app` WHERE `service_id` = OLD.`id`;
    DELETE FROM `mysql_rest_service_metadata`.`mrs_role` WHERE `specific_to_service_id` = OLD.`id`;
    DELETE FROM `mysql_rest_service_metadata`.`mrs_user_hierarchy_type` WHERE `specific_to_service_id` = OLD.`id`;
    DELETE FROM `mysql_rest_service_metadata`.`mrs_user_group` WHERE `specific_to_service_id` = OLD.`id`;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`db_schema_BEFORE_INSERT` BEFORE INSERT ON `db_schema` FOR EACH ROW
BEGIN
    SET @service_path := (SELECT CONCAT(COALESCE(se.in_development->>'$.developers', ''), h.name, se.url_context_root) AS path
        FROM `mysql_rest_service_metadata`.service se
            LEFT JOIN `mysql_rest_service_metadata`.url_host h
                ON se.url_host_id = h.id
        WHERE se.id = NEW.service_id);
    SET @validPath := (SELECT `mysql_rest_service_metadata`.`valid_request_path`(CONCAT(@service_path, NEW.request_path)));

    IF @validPath = 0 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "The request_path is already used by another entity.";
    END IF;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`db_schema_BEFORE_UPDATE` BEFORE UPDATE ON `db_schema` FOR EACH ROW
BEGIN
    IF (NEW.request_path <> OLD.request_path OR NEW.service_id <> OLD.service_id) THEN
        SET @service_path := (SELECT CONCAT(COALESCE(se.in_development->>'$.developers', ''), h.name, se.url_context_root) AS path
            FROM `mysql_rest_service_metadata`.service se
                LEFT JOIN `mysql_rest_service_metadata`.url_host h
                    ON se.url_host_id = h.id
            WHERE se.id = NEW.service_id);
        SET @validPath := (SELECT `mysql_rest_service_metadata`.`valid_request_path`(CONCAT(@service_path, NEW.request_path)));

        IF @validPath = 0 THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "The request_path is already used by another entity.";
        END IF;
    END IF;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`db_schema_BEFORE_DELETE` BEFORE DELETE ON `db_schema` FOR EACH ROW
BEGIN
    DELETE FROM `mysql_rest_service_metadata`.`db_object` WHERE `db_schema_id` = OLD.`id`;
    DELETE FROM `mysql_rest_service_metadata`.`mrs_privilege` WHERE `db_schema_id` = OLD.`id`;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`db_object_BEFORE_INSERT` BEFORE INSERT ON `db_object` FOR EACH ROW
BEGIN
    SET @schema_path := (SELECT CONCAT(COALESCE(se.in_development->>'$.developers', ''), h.name, se.url_context_root, sc.request_path) AS path
        FROM `mysql_rest_service_metadata`.db_schema sc
            LEFT OUTER JOIN `mysql_rest_service_metadata`.service se
                ON se.id = sc.service_id
            LEFT JOIN `mysql_rest_service_metadata`.url_host h
                ON se.url_host_id = h.id
        WHERE sc.id = NEW.db_schema_id);
    SET @validPath := (SELECT `mysql_rest_service_metadata`.`valid_request_path`(CONCAT(@schema_path, NEW.request_path)));

    IF @validPath = 0 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "The request_path is already used by another entity.";
    END IF;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`db_object_BEFORE_UPDATE` BEFORE UPDATE ON `db_object` FOR EACH ROW
BEGIN
    IF (NEW.request_path <> OLD.request_path OR NEW.db_schema_id <> OLD.db_schema_id) THEN
        SET @schema_path := (SELECT CONCAT(COALESCE(se.in_development->>'$.developers', ''), h.name, se.url_context_root, sc.request_path) AS path
            FROM `mysql_rest_service_metadata`.db_schema sc
                LEFT OUTER JOIN `mysql_rest_service_metadata`.service se
                    ON se.id = sc.service_id
                LEFT JOIN `mysql_rest_service_metadata`.url_host h
                    ON se.url_host_id = h.id
            WHERE sc.id = NEW.db_schema_id);
        SET @validPath := (SELECT `mysql_rest_service_metadata`.`valid_request_path`(CONCAT(@schema_path, NEW.request_path)));

        IF @validPath = 0 THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "The request_path is already used by another entity.";
        END IF;
    END IF;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`db_object_BEFORE_DELETE` BEFORE DELETE ON `db_object` FOR EACH ROW
BEGIN
    DELETE FROM `mysql_rest_service_metadata`.`mrs_privilege` WHERE `db_object_id` = OLD.`id`;
    DELETE FROM `mysql_rest_service_metadata`.`mrs_db_object_row_group_security` WHERE `db_object_id` = OLD.`id`;
    DELETE FROM `mysql_rest_service_metadata`.`object` WHERE `db_object_id` = OLD.`id`;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`auth_vendor_BEFORE_DELETE` BEFORE DELETE ON `auth_vendor` FOR EACH ROW
BEGIN
    DELETE FROM `mysql_rest_service_metadata`.`auth_app` WHERE `auth_vendor_id` = OLD.`id`;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`auth_app_BEFORE_DELETE` BEFORE DELETE ON `auth_app` FOR EACH ROW
BEGIN
    DELETE FROM `mysql_rest_service_metadata`.`mrs_user` WHERE `auth_app_id` = OLD.`id`;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`mrs_user_BEFORE_INSERT` BEFORE INSERT ON `mrs_user` FOR EACH ROW
BEGIN
    IF NEW.name IS NOT NULL AND (SELECT COUNT(*) FROM `mysql_rest_service_metadata`.`mrs_user` AS u
        WHERE UPPER(u.name) = UPPER(NEW.name) AND u.auth_app_id = NEW.auth_app_id AND NEW.id <> u.id) > 0
    THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "This name has already been used.";
    END IF;
    IF NEW.email IS NOT NULL AND (SELECT COUNT(*) FROM `mysql_rest_service_metadata`.`mrs_user` AS u
        WHERE UPPER(u.email) = UPPER(NEW.email) AND u.auth_app_id = NEW.auth_app_id AND NEW.id <> u.id) > 0
    THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "This email has already been used.";
    END IF;
    IF (NEW.auth_string IS NULL AND
        (SELECT a.auth_vendor_id FROM `mysql_rest_service_metadata`.`auth_app` AS a WHERE a.id = NEW.auth_app_id) = 0x30000000000000000000000000000000)
    THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "A this account requires a password to be set.";
    END IF;
    IF JSON_STORAGE_SIZE(NEW.app_options) > 16384 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "The JSON value stored in app_options must not be bigger than 16KB.";
    END IF;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`mrs_user_BEFORE_UPDATE` BEFORE UPDATE ON `mrs_user` FOR EACH ROW
BEGIN
    IF NEW.name IS NOT NULL AND (SELECT COUNT(*) FROM `mysql_rest_service_metadata`.`mrs_user` AS u
        WHERE UPPER(u.name) = UPPER(NEW.name) AND u.auth_app_id = NEW.auth_app_id AND NEW.id <> u.id) > 0
    THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "This name has already been used.";
    END IF;
    IF NEW.email IS NOT NULL AND (SELECT COUNT(*) FROM `mysql_rest_service_metadata`.`mrs_user` AS u
        WHERE UPPER(u.email) = UPPER(NEW.email) AND u.auth_app_id = NEW.auth_app_id AND NEW.id <> u.id) > 0
    THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "This email has already been used.";
    END IF;
    IF (NEW.auth_string IS NULL AND
        (SELECT a.auth_vendor_id FROM `mysql_rest_service_metadata`.`auth_app` AS a WHERE a.id = NEW.auth_app_id) = 0x30000000000000000000000000000000)
    THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "A this account requires a password to be set.";
    END IF;
    IF JSON_STORAGE_SIZE(NEW.app_options) > 16384 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "The JSON value stored in app_options must not be bigger than 16KB.";
    END IF;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`mrs_user_BEFORE_DELETE` BEFORE DELETE ON `mrs_user` FOR EACH ROW
BEGIN
    DELETE FROM `mysql_rest_service_metadata`.`mrs_user_hierarchy` WHERE `user_id` = OLD.`id` OR `reporting_to_user_id` = OLD.`id`;
    DELETE FROM `mysql_rest_service_metadata`.`mrs_user_has_role` WHERE `user_id` = OLD.`id`;
    DELETE FROM `mysql_rest_service_metadata`.`mrs_user_has_group` WHERE `user_id` = OLD.`id`;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`content_set_BEFORE_INSERT` BEFORE INSERT ON `content_set` FOR EACH ROW
BEGIN
    SET @service_path := (SELECT CONCAT(COALESCE(se.in_development->>'$.developers', ''), h.name, se.url_context_root) AS path
        FROM `mysql_rest_service_metadata`.service se
            LEFT JOIN `mysql_rest_service_metadata`.url_host h
                ON se.url_host_id = h.id
        WHERE se.id = NEW.service_id);
    SET @validPath := (SELECT `mysql_rest_service_metadata`.`valid_request_path`(CONCAT(@service_path, NEW.request_path)));

    IF @validPath = 0 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "The request_path is already used by another entity.";
    END IF;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`content_set_BEFORE_UPDATE` BEFORE UPDATE ON `content_set` FOR EACH ROW
BEGIN
    IF (NEW.request_path <> OLD.request_path OR NEW.service_id <> OLD.service_id) THEN
        SET @service_path := (SELECT CONCAT(COALESCE(se.in_development->>'$.developers', ''), h.name, se.url_context_root) AS path
            FROM `mysql_rest_service_metadata`.service se
                LEFT JOIN `mysql_rest_service_metadata`.url_host h
                    ON se.url_host_id = h.id
            WHERE se.id = NEW.service_id);
        SET @validPath := (SELECT `mysql_rest_service_metadata`.`valid_request_path`(CONCAT(@service_path, NEW.request_path)));

        IF @validPath = 0 THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "The request_path is already used by another entity.";
        END IF;
    END IF;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`content_set_BEFORE_DELETE` BEFORE DELETE ON `content_set` FOR EACH ROW
BEGIN
    DELETE FROM `mysql_rest_service_metadata`.`content_file`
    WHERE `content_set_id` = OLD.`id`;
    DELETE FROM `mysql_rest_service_metadata`.`content_set_has_obj_def`
    WHERE `content_set_id` = OLD.`id`;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`content_file_BEFORE_INSERT` BEFORE INSERT ON `content_file` FOR EACH ROW
BEGIN
    SET @content_set_path := (SELECT CONCAT(h.name, se.url_context_root, co.request_path) AS path
        FROM `mysql_rest_service_metadata`.content_set co
            LEFT OUTER JOIN `mysql_rest_service_metadata`.service se
                ON se.id = co.service_id
            LEFT JOIN `mysql_rest_service_metadata`.url_host h
                ON se.url_host_id = h.id
        WHERE co.id = NEW.content_set_id);
    SET @validPath := (SELECT `mysql_rest_service_metadata`.`valid_request_path`(CONCAT(@content_set_path, NEW.request_path)));

    IF @validPath = 0 THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "The request_path is already used by another entity.";
    END IF;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`content_file_BEFORE_UPDATE` BEFORE UPDATE ON `content_file` FOR EACH ROW
BEGIN
    IF (NEW.request_path <> OLD.request_path OR NEW.content_set_id <> OLD.content_set_id) THEN
        SET @content_set_path := (SELECT CONCAT(h.name, se.url_context_root, co.request_path) AS path
            FROM `mysql_rest_service_metadata`.content_set co
                LEFT OUTER JOIN `mysql_rest_service_metadata`.service se
                    ON se.id = co.service_id
                LEFT JOIN `mysql_rest_service_metadata`.url_host h
                    ON se.url_host_id = h.id
            WHERE co.id = NEW.content_set_id);
        SET @validPath := (SELECT `mysql_rest_service_metadata`.`valid_request_path`(CONCAT(@content_set_path, NEW.request_path)));

        IF @validPath = 0 THEN
            SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = "The request_path is already used by another entity.";
        END IF;
    END IF;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`mrs_role_BEFORE_DELETE` BEFORE DELETE ON `mrs_role` FOR EACH ROW
BEGIN
    DELETE FROM `mysql_rest_service_metadata`.`mrs_user_has_role` WHERE `role_id` = OLD.`id`;
    # Workaround to fix issue with recursive delete
    IF OLD.id <> NULL THEN
        DELETE FROM `mysql_rest_service_metadata`.`mrs_role` WHERE `derived_from_role_id` = OLD.`id`;
    END IF;
    DELETE FROM `mysql_rest_service_metadata`.`mrs_privilege` WHERE `role_id` = OLD.`id`;
    DELETE FROM `mysql_rest_service_metadata`.`mrs_user_group_has_role` WHERE `role_id` = OLD.`id`;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`mrs_user_hierarchy_type_BEFORE_DELETE` BEFORE DELETE ON `mrs_user_hierarchy_type` FOR EACH ROW
BEGIN
    DELETE FROM `mysql_rest_service_metadata`.`mrs_user_hierarchy` WHERE `user_hierarchy_type_id` = OLD.`id`;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`mrs_user_group_BEFORE_DELETE` BEFORE DELETE ON `mrs_user_group` FOR EACH ROW
BEGIN
    DELETE FROM `mysql_rest_service_metadata`.`mrs_user_has_group` WHERE `user_group_id` = OLD.`id`;
    DELETE FROM `mysql_rest_service_metadata`.`mrs_user_group_hierarchy` WHERE `user_group_id` = OLD.`id` OR `parent_group_id` = OLD.`id`;
    DELETE FROM `mysql_rest_service_metadata`.`mrs_user_group_has_role` WHERE `user_group_id` = OLD.`id`;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`mrs_group_hierarchy_type_BEFORE_DELETE` BEFORE DELETE ON `mrs_group_hierarchy_type` FOR EACH ROW
BEGIN
    DELETE FROM `mysql_rest_service_metadata`.`mrs_user_group_hierarchy` WHERE `group_hierarchy_type_id` = OLD.`id`;
    DELETE FROM `mysql_rest_service_metadata`.`mrs_db_object_row_group_security` WHERE `group_hierarchy_type_id` = OLD.`id`;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`router_BEFORE_DELETE` BEFORE DELETE ON `router` FOR EACH ROW
BEGIN
    DELETE FROM `mysql_rest_service_metadata`.`router_status` WHERE `router_id` = OLD.`id`;
    DELETE FROM `mysql_rest_service_metadata`.`router_general_log` WHERE `router_id` = OLD.`id`;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`router_session_BEFORE_DELETE` BEFORE DELETE ON `router_session` FOR EACH ROW
BEGIN
    DELETE FROM `mysql_rest_service_metadata`.`router_general_log` WHERE `router_session_id` = OLD.`id`;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`object_BEFORE_DELETE` BEFORE DELETE ON `object` FOR EACH ROW
BEGIN
    DELETE FROM `mysql_rest_service_metadata`.`object_field` WHERE `object_id` = OLD.`id`;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`object_field_BEFORE_DELETE` BEFORE DELETE ON `object_field` FOR EACH ROW
BEGIN
    SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0;
    DELETE FROM `mysql_rest_service_metadata`.`object_reference` WHERE `id` = OLD.`represents_reference_id`;
    SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`service_has_auth_app_AFTER_DELETE` AFTER DELETE ON `service_has_auth_app` FOR EACH ROW
BEGIN
    # Since FKs do not fire the triggers on the related tables, manually trigger the DELETEs
    # If the corresponding auth_app is not used by another service, delete it
    IF ((SELECT COUNT(*) FROM `mysql_rest_service_metadata`.`service_has_auth_app` WHERE auth_app_id = OLD.auth_app_id) = 0) THEN
        DELETE FROM `mysql_rest_service_metadata`.`auth_app` WHERE `id` = OLD.`auth_app_id`;
    END IF;
END$$

USE `mysql_rest_service_metadata`$$
CREATE DEFINER = CURRENT_USER TRIGGER `mysql_rest_service_metadata`.`content_set_has_obj_def_AFTER_DELETE` AFTER DELETE ON `content_set_has_obj_def` FOR EACH ROW
BEGIN
    DELETE FROM `mysql_rest_service_metadata`.`db_object` dbo
    WHERE OLD.kind = "Script" AND dbo.id = OLD.db_object_id;
END$$


DELIMITER ;$$

SET SQL_MODE=@OLD_SQL_MODE;
SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS;
SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS;

# -----------------------------------------------------
# Data for table `mysql_rest_service_metadata`.`url_host`
# -----------------------------------------------------
START TRANSACTION;
USE `mysql_rest_service_metadata`;
INSERT INTO `mysql_rest_service_metadata`.`url_host` (`id`, `name`, `comments`) VALUES (0x31, '', 'Match any host');

COMMIT;


# -----------------------------------------------------
# Data for table `mysql_rest_service_metadata`.`auth_vendor`
# -----------------------------------------------------
START TRANSACTION;
USE `mysql_rest_service_metadata`;
INSERT INTO `mysql_rest_service_metadata`.`auth_vendor` (`id`, `name`, `validation_url`, `enabled`, `comments`, `options`) VALUES (0x30, 'MRS', NULL, 1, 'Built-in user management of MRS', NULL);
INSERT INTO `mysql_rest_service_metadata`.`auth_vendor` (`id`, `name`, `validation_url`, `enabled`, `comments`, `options`) VALUES (0x31, 'MySQL Internal', NULL, 1, 'Provides basic authentication via MySQL Server accounts', NULL);
INSERT INTO `mysql_rest_service_metadata`.`auth_vendor` (`id`, `name`, `validation_url`, `enabled`, `comments`, `options`) VALUES (0x32, 'Facebook', NULL, 1, 'Uses the Facebook Login OAuth2 service', NULL);
INSERT INTO `mysql_rest_service_metadata`.`auth_vendor` (`id`, `name`, `validation_url`, `enabled`, `comments`, `options`) VALUES (0x34, 'Google', NULL, 1, 'Uses the Google OAuth2 service', NULL);
INSERT INTO `mysql_rest_service_metadata`.`auth_vendor` (`id`, `name`, `validation_url`, `enabled`, `comments`, `options`) VALUES (0x35, 'OCI OAuth2', NULL, 1, 'Uses the OCI OAuth2 service', NULL);

COMMIT;


# -----------------------------------------------------
# Data for table `mysql_rest_service_metadata`.`config`
# -----------------------------------------------------
START TRANSACTION;
USE `mysql_rest_service_metadata`;
INSERT INTO `mysql_rest_service_metadata`.`config` (`id`, `service_enabled`, `data`) VALUES (1, 1, '{ \"defaultStaticContent\": { \"index.html\": \"PCFET0NUWVBFIGh0bWw+PGh0bWw+PGhlYWQ+PHRpdGxlPk15U1FMIFJFU1QgU2VydmljZTwvdGl0bGU+PGxpbmsgcmVsPSJpY29uIiB0eXBlPSJpbWFnZS9zdmcreG1sIiBocmVmPSIuL2Zhdmljb24uc3ZnIj48bWV0YSBjaGFyc2V0PSJ1dGYtOCI+PG1ldGEgbmFtZT0idmlld3BvcnQiIGNvbnRlbnQ9IndpZHRoPWRldmljZS13aWR0aCxpbml0aWFsLXNjYWxlPTEiPjxzdHlsZT46cm9vdHstLWJvZHktYmFja2dyb3VuZDpoc2woMjQwLCA1JSwgOTElKTstLWJvZHktdGV4dC1jb2xvcjpoc2woMjQwLCA1JSwgMTIlKTstLWljb24tY29sb3I6aHNsKDIwMCwgNjUlLCA0MCUpOy0tdGV4dExpbmstZm9yZWdyb3VuZDpoc2woMjAwLCA2NSUsIDM0JSk7LS1tYWluLWJhY2tncm91bmQtY29sb3I6I2ZmZjstLXNlY29uZGFyeS1iYWNrZ3JvdW5kLWNvbG9yOiNmZmY7LS10b29sYmFyLWJhY2tncm91bmQtY29sb3I6I2Y1ZjVmNzstLXRvb2xiYXItc2Vjb25kYXJ5LWJhY2tncm91bmQtY29sb3I6I2Y4ZjhmODstLWxpc3QtYmFja2dyb3VuZC1jb2xvcjojRjJGMkY3Oy0tbGlzdC1pdGVtLWJhY2tncm91bmQtY29sb3I6I2ZmZjstLWxpc3QtaXRlbS1zZWxlY3RlZC1iYWNrZ3JvdW5kLWNvbG9yOmhzbCgyNDAsIDglLCA4OCUpOy0tbGlzdC1pdGVtLXNlbGVjdGVkLWZvY3VzLWJhY2tncm91bmQtY29sb3I6aHNsKDIwMCwgNjUlLCA4NiUpOy0tYnV0dG9uLWJhY2tncm91bmQtY29sb3I6IzZFNkQ3MDstLXByaW1hcnktdGV4dC1jb2xvcjojMzMzMzMzOy0tc2Vjb25kYXJ5LXRleHQtY29sb3I6I0FEQURBRDstLXRpdGxlLXRleHQtY29sb3I6IzIyMjstLWxpc3QtaXRlbS1jb2xvcjojMDAwOy0tbGlzdC1pdGVtLXNlY29uZGFyeS1jb2xvcjojNjY2Oy0tYnV0dG9uLXRleHQtY29sb3I6I0RGREVERjstLXNwbGl0dGVyLWNvbG9yOiNENEQ0RDQ7LS1pY29uLWNvbG9yOmhzbCgyMDAsIDY1JSwgNDAlKTstLWZvY3VzLWNvbG9yOmhzbCgyMDAsIDY1JSwgNzAlKTstLWVycm9yLWNvbG9yOmhzbCgwLCAxMDAlLCA3MCUpOy0tZXJyb3ItdGV4dC1jb2xvcjojNTAwfUBtZWRpYSAocHJlZmVycy1jb2xvci1zY2hlbWU6ZGFyayl7OnJvb3R7LS1ib2R5LWJhY2tncm91bmQ6aHNsKDAsIDAlLCAxNyUpOy0tYm9keS10ZXh0LWNvbG9yOmhzbCgwLCAwJSwgNzUlKTstLXRleHRMaW5rLWZvcmVncm91bmQ6aHNsKDIwMCwgNjUlLCA1NCUpOy0tbWFpbi1iYWNrZ3JvdW5kLWNvbG9yOiMwZDBkMGQ7LS1zZWNvbmRhcnktYmFja2dyb3VuZC1jb2xvcjojMTgxQTFCOy0tdG9vbGJhci1iYWNrZ3JvdW5kLWNvbG9yOiMxRDIwMjE7LS10b29sYmFyLXNlY29uZGFyeS1iYWNrZ3JvdW5kLWNvbG9yOiMxODFBMUI7LS1saXN0LWJhY2tncm91bmQtY29sb3I6IzFFMjAyMjstLWxpc3QtaXRlbS1iYWNrZ3JvdW5kLWNvbG9yOiMxODFBMUI7LS1saXN0LWl0ZW0tc2VsZWN0ZWQtYmFja2dyb3VuZC1jb2xvcjpoc2woMjEwLCA3JSwgMjAlKTstLWxpc3QtaXRlbS1zZWxlY3RlZC1mb2N1cy1iYWNrZ3JvdW5kLWNvbG9yOmhzbCgyMDAsIDY1JSwgMjAlKTstLWJ1dHRvbi1iYWNrZ3JvdW5kLWNvbG9yOiM2NjY7LS1wcmltYXJ5LXRleHQtY29sb3I6I2M0YzRjNDstLXNlY29uZGFyeS10ZXh0LWNvbG9yOiM1OTU5NTk7LS10aXRsZS10ZXh0LWNvbG9yOiNlNWU1ZTc7LS1saXN0LWl0ZW0tY29sb3I6I0U4RTZFMzstLWxpc3QtaXRlbS1zZWNvbmRhcnktY29sb3I6IzY2NjstLXNwbGl0dGVyLWNvbG9yOiMzQTM0MzY7LS1pY29uLWNvbG9yOmhzbCgyMDAsIDY1JSwgNDAlKTstLWZvY3VzLWNvbG9yOmhzbCgyMDAsIDY1JSwgMzAlKTstLWVycm9yLWNvbG9yOmhzbCgwLCAxMDAlLCAyMCUpOy0tZXJyb3ItdGV4dC1jb2xvcjojZTAwfX1ib2R5LGh0bWx7d2lkdGg6MTAwJTtoZWlnaHQ6MTAwJX0qe21hcmdpbjowO3BhZGRpbmc6MH1ib2R5e292ZXJmbG93OmhpZGRlbjtiYWNrZ3JvdW5kLWNvbG9yOnZhcigtLWJvZHktYmFja2dyb3VuZCk7Zm9udC1mYW1pbHk6IkhlbHZldGljYSBOZXVlIixIZWx2ZXRpY2EsQXJpYWwsc2Fucy1zZXJpZjtmb250LXNpemU6MTJweDtjb2xvcjp2YXIoLS1ib2R5LXRleHQtY29sb3IpfWgye21hcmdpbjoyMHB4IDA7Zm9udC13ZWlnaHQ6MTAwO2ZvbnQtc2l6ZTozM3B4fXB7bGluZS1oZWlnaHQ6MTlweDtmb250LXdlaWdodDoyMDA7Zm9udC1zaXplOjE1cHh9YXtjb2xvcjp2YXIoLS10ZXh0TGluay1mb3JlZ3JvdW5kKTt0ZXh0LWRlY29yYXRpb246bm9uZTtmb250LXdlaWdodDo1MDA7cGFkZGluZzowIDIwcHg7Zm9udC1zaXplOjE1cHh9I3Jvb3R7ZGlzcGxheTpmbGV4O3dpZHRoOjEwMCU7aGVpZ2h0OjEwMCU7ZmxleC1kaXJlY3Rpb246Y29sdW1uO2FsaWduLWl0ZW1zOmNlbnRlcjtqdXN0aWZ5LWNvbnRlbnQ6Y2VudGVyfS53ZWxjb21lTG9nb3ttYXJnaW4tdG9wOjIwcHg7d2lkdGg6MTYwcHg7aGVpZ2h0OjE2MHB4O21pbi1oZWlnaHQ6MTYwcHg7YmFja2dyb3VuZC1jb2xvcjp2YXIoLS1pY29uLWNvbG9yKTstd2Via2l0LW1hc2staW1hZ2U6dXJsKHNha2lsYS5zdmcpO21hc2staW1hZ2U6dXJsKHNha2lsYS5zdmcpfS53ZWxjb21lVGV4dHtkaXNwbGF5OmZsZXg7ZmxleC1kaXJlY3Rpb246Y29sdW1uO2FsaWduLWl0ZW1zOmNlbnRlcjtqdXN0aWZ5LWNvbnRlbnQ6Y2VudGVyfS53ZWxjb21lVGV4dCBwe3RleHQtYWxpZ246Y2VudGVyfS53ZWxjb21lTGlua3N7cGFkZGluZzozMHB4IDB9LndlbGNvbWVTcGFjZXJ7aGVpZ2h0OjgwcHh9LmZvb3Rlcntwb3NpdGlvbjphYnNvbHV0ZTtib3R0b206MDtsaW5lLWhlaWdodDoxMnB0O2ZvbnQtd2VpZ2h0OjIwMDtmb250LXNpemU6MTBweDttYXJnaW46NXB4IDB9Lm1yc0xvZ2lue2Rpc3BsYXk6ZmxleDtmbGV4LWRpcmVjdGlvbjpjb2x1bW47cGFkZGluZy10b3A6MjBweDtwYWRkaW5nLWJvdHRvbToyMHB4O2dhcDoxMnB4O2FsaWduLWl0ZW1zOmNlbnRlcn0ubXJzTG9naW4gcHtmb250LXNpemU6MjBweDtmb250LXdlaWdodDo0MDB9Lm1yc0xvZ2luRmllbGRze2Rpc3BsYXk6ZmxleDtmbGV4LWRpcmVjdGlvbjpjb2x1bW59Lm1yc0xvZ2luRmllbGRzIGlucHV0W3R5cGU9cGFzc3dvcmRdLC5tcnNMb2dpbkZpZWxkcyBpbnB1dFt0eXBlPXRleHRde2JvcmRlcjpub25lO291dGxpbmU6MDtiYWNrZ3JvdW5kLWNvbG9yOnRyYW5zcGFyZW50O2NvbG9yOnZhcigtLXByaW1hcnktdGV4dC1jb2xvcik7Zm9udC1zaXplOjE3cHg7Zm9udC13ZWlnaHQ6MzAwO3dpZHRoOjI1MHB4fS5tcnNMb2dpbkZpZWxke2Rpc3BsYXk6ZmxleDtmbGV4LWRpcmVjdGlvbjpyb3c7Ym9yZGVyOjFweCBzb2xpZCB2YXIoLS1zZWNvbmRhcnktdGV4dC1jb2xvcik7cGFkZGluZzo4cHggOHB4IDhweCAxNnB4fS5tcnNMb2dpbkZpZWxkOmZpcnN0LW9mLXR5cGV7Ym9yZGVyLXRvcC1sZWZ0LXJhZGl1czo1cHg7Ym9yZGVyLXRvcC1yaWdodC1yYWRpdXM6NXB4fS5tcnNMb2dpbkZpZWxkOmxhc3Qtb2YtdHlwZTpub3QoOm9ubHktb2YtdHlwZSl7Ym9yZGVyLXRvcDowfS5tcnNMb2dpbkZpZWxkOmxhc3Qtb2YtdHlwZXtib3JkZXItYm90dG9tLWxlZnQtcmFkaXVzOjVweDtib3JkZXItYm90dG9tLXJpZ2h0LXJhZGl1czo1cHg7bWFyZ2luLWJvdHRvbToxNnB4fS5tcnNMb2dpbkZpZWxkIGlucHV0e2hlaWdodDoyNnB4fS5tcnNMb2dpbkJ0bk5leHR7Ym9yZGVyOjFweCBzb2xpZCB2YXIoLS1zZWNvbmRhcnktdGV4dC1jb2xvcik7Ym9yZGVyLXJhZGl1czo1MCU7d2lkdGg6MjRweDtoZWlnaHQ6MjRweDttYXJnaW4tbGVmdDoxMnB4O3BhZGRpbmctcmlnaHQ6MH0ubXJzTG9naW5CdG5OZXh0LmFjdGl2ZXtib3JkZXI6MXB4IHNvbGlkIHZhcigtLXByaW1hcnktdGV4dC1jb2xvcil9Lm1yc0xvZ2luQnRuTmV4dC5sb2FkaW5ne2JvcmRlcjpub25lO3dpZHRoOjI2cHh9Lm1yc0xvZ2luQnRuTmV4dDo6YWZ0ZXJ7Y29udGVudDoiIjtwb3NpdGlvbjpyZWxhdGl2ZTtib3JkZXI6c29saWQgdmFyKC0tc2Vjb25kYXJ5LXRleHQtY29sb3IpO2JvcmRlci13aWR0aDowIDNweCAzcHggMDtkaXNwbGF5OmlubGluZS1ibG9jaztwYWRkaW5nOjNweDt0cmFuc2Zvcm06cm90YXRlKC00NWRlZyk7LXdlYmtpdC10cmFuc2Zvcm06cm90YXRlKC00NWRlZyk7bWFyZ2luLWxlZnQ6NnB4O21hcmdpbi10b3A6N3B4fS5tcnNMb2dpbkJ0bk5leHQuYWN0aXZlOjphZnRlcntjb250ZW50OiIiO3Bvc2l0aW9uOnJlbGF0aXZlO2JvcmRlcjpzb2xpZCB2YXIoLS1wcmltYXJ5LXRleHQtY29sb3IpO2JvcmRlci13aWR0aDowIDNweCAzcHggMDtkaXNwbGF5OmlubGluZS1ibG9jaztwYWRkaW5nOjNweDt0cmFuc2Zvcm06cm90YXRlKC00NWRlZyk7LXdlYmtpdC10cmFuc2Zvcm06cm90YXRlKC00NWRlZyk7bWFyZ2luLWxlZnQ6NnB4O21hcmdpbi10b3A6N3B4fS5tcnNMb2dpbkJ0bk5leHQubG9hZGluZzo6YWZ0ZXJ7Y29udGVudDoiICI7ZGlzcGxheTpibG9jazt3aWR0aDoxN3B4O2hlaWdodDoxN3B4O21hcmdpbi1sZWZ0Oi0zcHg7bWFyZ2luLXRvcDotMnB4O2JvcmRlci1yYWRpdXM6NTAlO2JvcmRlcjo0cHggc29saWQgdmFyKC0tcHJpbWFyeS10ZXh0LWNvbG9yKTtib3JkZXItY29sb3I6dmFyKC0tcHJpbWFyeS10ZXh0LWNvbG9yKSB0cmFuc3BhcmVudCB2YXIoLS1wcmltYXJ5LXRleHQtY29sb3IpIHRyYW5zcGFyZW50O2FuaW1hdGlvbjptcnNMb2FkZXJLZXlmcmFtZXMgMS4ycyBsaW5lYXIgaW5maW5pdGV9QGtleWZyYW1lcyBtcnNMb2FkZXJLZXlmcmFtZXN7MCV7dHJhbnNmb3JtOnJvdGF0ZSgwKX0xMDAle3RyYW5zZm9ybTpyb3RhdGUoMzYwZGVnKX19Lm1yc0xvZ2luRXJyb3J7YmFja2dyb3VuZC1jb2xvcjp2YXIoLS1lcnJvci1jb2xvcik7Ym94LXNoYWRvdzpyZ2IoMCAwIDAgLyAxMCUpIDAgNXB4IDEwcHggMnB4O3dpZHRoOjIyMHB4O3BhZGRpbmc6OHB4IDIwcHggOHB4IDIwcHg7Ym9yZGVyOjFweCBzb2xpZCB2YXIoLS1lcnJvci10ZXh0LWNvbG9yKTtib3JkZXItcmFkaXVzOjZweDtvdmVyZmxvdy13cmFwOmJyZWFrLXdvcmR9Lm1yc0xvZ2luRXJyb3IgcHtjb2xvcjp2YXIoLS1lcnJvci10ZXh0LWNvbG9yKTtmb250LXNpemU6MTRweDt0ZXh0LWFsaWduOmNlbnRlcn0ubXJzTG9naW5FcnJvcjpiZWZvcmV7d2lkdGg6MTVweDtoZWlnaHQ6MTVweDtiYWNrZ3JvdW5kLWNvbG9yOnZhcigtLWVycm9yLWNvbG9yKTtjb250ZW50OiIiO3Bvc2l0aW9uOmFic29sdXRlO2xlZnQ6NTAlO21hcmdpbi10b3A6LTE2cHg7bWFyZ2luLWxlZnQ6LTE1cHg7dHJhbnNmb3JtOnJvdGF0ZSgxMzVkZWcpIHNrZXdYKDVkZWcpIHNrZXdZKDVkZWcpOy13ZWJraXQtdHJhbnNmb3JtOnJvdGF0ZSgxMzVkZWcpIHNrZXdYKDVkZWcpIHNrZXdZKDVkZWcpO2JvcmRlci1sZWZ0OjFweCBzb2xpZCB2YXIoLS1lcnJvci10ZXh0LWNvbG9yKTtib3JkZXItYm90dG9tOjFweCBzb2xpZCB2YXIoLS1lcnJvci10ZXh0LWNvbG9yKX0ubXJzTG9naW5TZXBhcmF0b3J7YmFja2dyb3VuZDpsaW5lYXItZ3JhZGllbnQodG8gcmlnaHQscmdiYSgyMDAsMjAwLDIwMCwwKSwjYzhjOGM4LCNjOGM4YzgscmdiYSgyMDAsMjAwLDIwMCwwKSk7d2lkdGg6NDAwcHg7aGVpZ2h0OjFweDttYXJnaW4tdG9wOjIwcHh9PC9zdHlsZT48L2hlYWQ+PGJvZHk+PGRpdiBpZD0icm9vdCI+PC9kaXY+PHNjcmlwdCB0eXBlPSJtb2R1bGUiPmltcG9ydHtoLHJlbmRlcixDb21wb25lbnQsY3JlYXRlUmVmLGh0bWx9ZnJvbSIuL3N0YW5kYWxvbmUtcHJlYWN0LmpzIjtjbGFzcyBNcnNCYXNlU2Vzc2lvbntsb2dpblN0YXRlPXt9O2NvbnN0cnVjdG9yKHQsZSxzPThlMyl7dGhpcy5zZXJ2aWNlVXJsPXQsdGhpcy5hdXRoUGF0aD1lPz8iL2F1dGhlbnRpY2F0aW9uIix0aGlzLmRlZmF1bHRUaW1lb3V0PXN9ZG9GZXRjaD1hc3luYyh0LGUscyxhLGksbyk9Pnsib2JqZWN0Ij09dHlwZW9mIHQmJm51bGwhPT10PyhlPXQ/LmVycm9yTXNnPz8iRmFpbGVkIHRvIGZldGNoIGRhdGEuIixzPXQ/Lm1ldGhvZD8/IkdFVCIsYT10Py5ib2R5LG89dD8udGltZW91dCx0PXQ/LmlucHV0KTooZT1lPz8iRmFpbGVkIHRvIGZldGNoIGRhdGEuIixzPXM/PyJHRVQiKTtjb25zdCByPW5ldyBBYm9ydENvbnRyb2xsZXIsbj1zZXRUaW1lb3V0KCgoKT0+e3IuYWJvcnQoKX0pLG8/P3RoaXMuZGVmYXVsdFRpbWVvdXQpO2xldCBsO3RyeXtsPWF3YWl0IGZldGNoKGAke3RoaXMuc2VydmljZVVybD8/IiJ9JHt0fWAse21ldGhvZDpzLGhlYWRlcnM6dm9pZCAwIT09dGhpcy5hY2Nlc3NUb2tlbj97QXV0aG9yaXphdGlvbjoiQmVhcmVyICIrdGhpcy5hY2Nlc3NUb2tlbn06dm9pZCAwLGJvZHk6dm9pZCAwIT09YT9KU09OLnN0cmluZ2lmeShhKTp2b2lkIDAsc2lnbmFsOnIuc2lnbmFsfSl9Y2F0Y2gocyl7dGhyb3cgbmV3IEVycm9yKGAke2V9XG5cblBsZWFzZSBjaGVjayBpZiBNeVNRTCBSb3V0ZXIgaXMgcnVubmluZyBhbmQgdGhlIFJFU1QgZW5kcG9pbnQgJHt0aGlzLnNlcnZpY2VVcmw/PyIifSR7dH0gZG9lcyBleGlzdC5cblxuJHtzIGluc3RhbmNlb2YgRXJyb3I/cy5tZXNzYWdlOlN0cmluZyhzKX1gKX1maW5hbGx5e2NsZWFyVGltZW91dChuKX1pZighbC5vayYmaSl7aWYoNDAxPT09bC5zdGF0dXMpdGhyb3cgbmV3IEVycm9yKGBOb3QgYXV0aGVudGljYXRlZC4gUGxlYXNlIGF1dGhlbnRpY2F0ZSBmaXJzdCBiZWZvcmUgYWNjZXNzaW5nIHRoZSBwYXRoICR7dGhpcy5zZXJ2aWNlVXJsPz8iIn0ke3R9LmApO2xldCBzO3RyeXtzPWF3YWl0IGwuanNvbigpfWNhdGNoKHQpe3Rocm93IG5ldyBFcnJvcihgJHtsLnN0YXR1c30uICR7ZX0gKCR7bC5zdGF0dXNUZXh0fSlgKX10aHJvdyJzdHJpbmciPT10eXBlb2Ygcy5tZXNzYWdlP25ldyBFcnJvcihTdHJpbmcocy5tZXNzYWdlKSk6bmV3IEVycm9yKGAke2wuc3RhdHVzfS4gJHtlfSAoJHtsLnN0YXR1c1RleHR9KWArKHZvaWQgMCE9PXM/IlxuXG4iK0pTT04uc3RyaW5naWZ5KHMsbnVsbCw0KSsiXG4iOiIiKSl9cmV0dXJuIGx9O2dldEF1dGhBcHBzPWFzeW5jKCk9Pntjb25zdCB0PWF3YWl0IHRoaXMuZG9GZXRjaCh7aW5wdXQ6YCR7dGhpcy5hdXRoUGF0aH0vYXV0aEFwcHNgLHRpbWVvdXQ6M2UzLGVycm9yTXNnOiJGYWlsZWQgdG8gZmV0Y2ggQXV0aGVudGljYXRpb24gQXBwcy4ifSk7aWYodC5vayl7cmV0dXJuIGF3YWl0IHQuanNvbigpfXtsZXQgZT1udWxsO3RyeXtlPWF3YWl0IHQuanNvbigpfWNhdGNoKHQpe31jb25zdCBzPWBGYWlsZWQgdG8gZmV0Y2ggQXV0aGVudGljYXRpb24gQXBwcy5cblxuUGxlYXNlIGVuc3VyZSBNeVNRTCBSb3V0ZXIgaXMgcnVubmluZyBhbmQgdGhlIFJFU1QgZW5kcG9pbnQgJHtTdHJpbmcodGhpcy5zZXJ2aWNlVXJsKX0ke3RoaXMuYXV0aFBhdGh9L2F1dGhBcHBzIGlzIGFjY2Vzc2libGUuIGA7dGhyb3cgbmV3IEVycm9yKHMrYCgke3Quc3RhdHVzfToke3Quc3RhdHVzVGV4dH0pYCsobnVsbCE9ZT8iXG5cbiIrSlNPTi5zdHJpbmdpZnkoZSxudWxsLDQpKyJcbiI6IiIpKX19O2dldEF1dGhlbnRpY2F0aW9uU3RhdHVzPWFzeW5jKCk9Pnt0cnl7cmV0dXJuIGF3YWl0KGF3YWl0IHRoaXMuZG9GZXRjaCh7aW5wdXQ6Ii9hdXRoZW50aWNhdGlvbi9zdGF0dXMiLGVycm9yTXNnOiJGYWlsZWQgdG8gYXV0aGVudGljYXRlLiJ9KSkuanNvbigpfWNhdGNoKHQpe3JldHVybntzdGF0dXM6InVuYXV0aG9yaXplZCJ9fX07dmVyaWZ5VXNlck5hbWU9YXN5bmModCxlKT0+e3RoaXMuYXV0aEFwcD10O2NvbnN0IHM9dGhpcy5oZXgoY3J5cHRvLmdldFJhbmRvbVZhbHVlcyhuZXcgVWludDhBcnJheSgxMCkpKSxhPWF3YWl0KGF3YWl0IHRoaXMuZG9GZXRjaCh7aW5wdXQ6YCR7dGhpcy5hdXRoUGF0aH0vbG9naW4/YXBwPSR7dH1gLG1ldGhvZDoiUE9TVCIsYm9keTp7dXNlcjplLG5vbmNlOnN9fSkpLmpzb24oKTthLnNhbHQ9bmV3IFVpbnQ4QXJyYXkoYS5zYWx0KSx0aGlzLmxvZ2luU3RhdGU9e2NsaWVudEZpcnN0OmBuPSR7ZX0scj0ke3N9YCxjbGllbnRGaW5hbDpgcj0ke2Eubm9uY2V9YCxzZXJ2ZXJGaXJzdDp0aGlzLmJ1aWxkU2VydmVyRmlyc3QoYSksY2hhbGxlbmdlOmEsbG9naW5FcnJvcjp2b2lkIDB9fTt2ZXJpZnlQYXNzd29yZD1hc3luYyB0PT57Y29uc3R7Y2hhbGxlbmdlOmUsY2xpZW50Rmlyc3Q6cyxzZXJ2ZXJGaXJzdDphLGNsaWVudEZpbmFsOml9PXRoaXMubG9naW5TdGF0ZTtpZih2b2lkIDA9PT10fHwiIj09PXR8fHZvaWQgMD09PXRoaXMuYXV0aEFwcHx8dm9pZCAwPT09c3x8dm9pZCAwPT09YXx8dm9pZCAwPT09ZXx8dm9pZCAwPT09aSlyZXR1cm57YXV0aEFwcDp0aGlzLmF1dGhBcHAsZXJyb3JDb2RlOjEsZXJyb3JNZXNzYWdlOiJObyBwYXNzd29yZCBnaXZlbi4ifTt7Y29uc3Qgbz1uZXcgVGV4dEVuY29kZXIscj1gJHtzfSwke2F9LCR7aX1gLG49QXJyYXkuZnJvbShhd2FpdCB0aGlzLmNhbGN1bGF0ZUNsaWVudFByb29mKHQsZS5zYWx0LGUuaXRlcmF0aW9ucyxvLmVuY29kZShyKSkpO3RyeXtjb25zdCB0PWF3YWl0IHRoaXMuZG9GZXRjaCh7aW5wdXQ6YCR7dGhpcy5hdXRoUGF0aH0vbG9naW4/YXBwPSR7dGhpcy5hdXRoQXBwfSZzZXNzaW9uVHlwZT1iZWFyZXJgKyh2b2lkIDAhPT1lLnNlc3Npb24/IiZzZXNzaW9uPSIrZS5zZXNzaW9uOiIiKSxtZXRob2Q6IlBPU1QiLGJvZHk6e2NsaWVudFByb29mOm4sbm9uY2U6ZS5ub25jZSxzdGF0ZToicmVzcG9uc2UifX0sdm9pZCAwLHZvaWQgMCx2b2lkIDAsITEpO2lmKHQub2spe2NvbnN0IGU9YXdhaXQgdC5qc29uKCk7cmV0dXJuIHRoaXMuYWNjZXNzVG9rZW49U3RyaW5nKGUuYWNjZXNzVG9rZW4pLHthdXRoQXBwOnRoaXMuYXV0aEFwcCxqd3Q6dGhpcy5hY2Nlc3NUb2tlbn19cmV0dXJuIHRoaXMuYWNjZXNzVG9rZW49dm9pZCAwLHthdXRoQXBwOnRoaXMuYXV0aEFwcCxlcnJvckNvZGU6dC5zdGF0dXMsZXJyb3JNZXNzYWdlOjQwMT09PXQuc3RhdHVzPyJUaGUgc2lnbiBpbiBmYWlsZWQuIFBsZWFzZSBjaGVjayB5b3VyIHVzZXJuYW1lIGFuZCBwYXNzd29yZC4iOmBUaGUgc2lnbiBpbiBmYWlsZWQuIEVycm9yIGNvZGU6ICR7U3RyaW5nKHQuc3RhdHVzKX1gfX1jYXRjaCh0KXtyZXR1cm57YXV0aEFwcDp0aGlzLmF1dGhBcHAsZXJyb3JDb2RlOjIsZXJyb3JNZXNzYWdlOmBUaGUgc2lnbiBpbiBmYWlsZWQuIFNlcnZlciBFcnJvcjogJHtTdHJpbmcodCl9YH19fX07aGV4PXQ9PkFycmF5LmZyb20obmV3IFVpbnQ4QXJyYXkodCkpLm1hcCgodD0+dC50b1N0cmluZygxNikucGFkU3RhcnQoMiwiMCIpKSkuam9pbigiIik7YnVpbGRTZXJ2ZXJGaXJzdD10PT57Y29uc3QgZT13aW5kb3cuYnRvYShTdHJpbmcuZnJvbUNoYXJDb2RlLmFwcGx5KG51bGwsQXJyYXkuZnJvbSh0LnNhbHQpKSk7cmV0dXJuYHI9JHt0Lm5vbmNlfSxzPSR7ZX0saT0ke1N0cmluZyh0Lml0ZXJhdGlvbnMpfWB9O2NhbGN1bGF0ZVBia2RmMj1hc3luYyh0LGUscyk9Pntjb25zdCBhPWF3YWl0IGNyeXB0by5zdWJ0bGUuaW1wb3J0S2V5KCJyYXciLHQse25hbWU6IlBCS0RGMiJ9LCExLFsiZGVyaXZlS2V5IiwiZGVyaXZlQml0cyJdKTtyZXR1cm4gbmV3IFVpbnQ4QXJyYXkoYXdhaXQgY3J5cHRvLnN1YnRsZS5kZXJpdmVCaXRzKHtuYW1lOiJQQktERjIiLGhhc2g6IlNIQS0yNTYiLHNhbHQ6ZSxpdGVyYXRpb25zOnN9LGEsMjU2KSl9O2NhbGN1bGF0ZVNoYTI1Nj1hc3luYyB0PT5uZXcgVWludDhBcnJheShhd2FpdCBjcnlwdG8uc3VidGxlLmRpZ2VzdCgiU0hBLTI1NiIsdCkpO2NhbGN1bGF0ZUhtYWM9YXN5bmModCxlKT0+e2NvbnN0IHM9YXdhaXQgd2luZG93LmNyeXB0by5zdWJ0bGUuaW1wb3J0S2V5KCJyYXciLHQse25hbWU6IkhNQUMiLGhhc2g6e25hbWU6IlNIQS0yNTYifX0sITAsWyJzaWduIiwidmVyaWZ5Il0pLGE9YXdhaXQgd2luZG93LmNyeXB0by5zdWJ0bGUuc2lnbigiSE1BQyIscyxlKTtyZXR1cm4gbmV3IFVpbnQ4QXJyYXkoYSl9O2NhbGN1bGF0ZVhvcj0odCxlKT0+e2NvbnN0IHM9dC5sZW5ndGgsYT1lLmxlbmd0aDtsZXQgaSxvLHI7cz5hPyhpPW5ldyBVaW50OEFycmF5KHQpLG89ZSxyPWEpOihpPW5ldyBVaW50OEFycmF5KGUpLG89dCxyPXMpO2ZvcihsZXQgdD0wO3Q8cjsrK3QpaVt0XV49b1t0XTtyZXR1cm4gaX07Y2FsY3VsYXRlQ2xpZW50UHJvb2Y9YXN5bmModCxlLHMsYSk9Pntjb25zdCBpPW5ldyBUZXh0RW5jb2RlcixvPWF3YWl0IHRoaXMuY2FsY3VsYXRlUGJrZGYyKGkuZW5jb2RlKHQpLGUscykscj1hd2FpdCB0aGlzLmNhbGN1bGF0ZUhtYWMobyxpLmVuY29kZSgiQ2xpZW50IEtleSIpKSxuPWF3YWl0IHRoaXMuY2FsY3VsYXRlU2hhMjU2KHIpLGw9YXdhaXQgdGhpcy5jYWxjdWxhdGVIbWFjKG4sYSk7cmV0dXJuIHRoaXMuY2FsY3VsYXRlWG9yKGwscil9fWNsYXNzIE1yc0Jhc2VBcHAgZXh0ZW5kcyBDb21wb25lbnR7Y29uc3RydWN0b3IodCxlKXtzdXBlcigpLHQ/Pz0iIixlPz89e30sdGhpcy5zdGF0ZT17YXV0aGVudGljYXRpbmc6ITEscmVzdGFydGluZzohMSwuLi5lfSxnbG9iYWxUaGlzLmFkZEV2ZW50TGlzdGVuZXIoImhhc2hjaGFuZ2UiLCgoKT0+e3RoaXMuZm9yY2VVcGRhdGUoKX0pLCExKSx2b2lkIDAhPT1lLnNlcnZpY2UmJih0aGlzLnNlc3Npb249bmV3IE1yc0Jhc2VTZXNzaW9uKGUuc2VydmljZSxlLmF1dGhQYXRoKSx0aGlzLmhhbmRsZUxvZ2luKHZvaWQgMCx2b2lkIDAsITApKX1nZXRVcmxXaXRoTmV3U2VhcmNoU3RyaW5nPSh0PSIiKT0+Z2xvYmFsVGhpcy5sb2NhdGlvbi5wcm90b2NvbCsiLy8iK2dsb2JhbFRoaXMubG9jYXRpb24uaG9zdCtnbG9iYWxUaGlzLmxvY2F0aW9uLnBhdGhuYW1lKygiIiE9PXQ/YD8ke3R9YDoiIikrZ2xvYmFsVGhpcy5sb2NhdGlvbi5oYXNoO3N0YXJ0TG9naW49dD0+e2lmKHZvaWQgMCE9PXQpe2NvbnN0IGU9ZW5jb2RlVVJJQ29tcG9uZW50KE1yc0Jhc2VBcHAuZ2V0VXJsV2l0aE5ld1NlYXJjaFN0cmluZyhgYXV0aEFwcD0ke3R9YCkpO2dsb2JhbFRoaXMubG9jYXRpb24uaHJlZj1gJHt0aGlzLnNlcnZpY2VVcmx9L2F1dGhlbnRpY2F0aW9uL2xvZ2luP2FwcD0ke3R9JnNlc3Npb25UeXBlPWJlYXJlciZvbkNvbXBsZXRpb25SZWRpcmVjdD0ke2V9YH1lbHNlIGdsb2JhbFRoaXMubG9jYXRpb24uaHJlZj1NcnNCYXNlQXBwLmdldFVybFdpdGhOZXdTZWFyY2hTdHJpbmcoKX07bG9nb3V0PSgpPT57Z2xvYmFsVGhpcy5sb2NhbFN0b3JhZ2UucmVtb3ZlSXRlbShgJHt0aGlzLmFwcE5hbWV9Snd0QWNjZXNzVG9rZW5gKSxnbG9iYWxUaGlzLmxvY2FsU3RvcmFnZS5yZW1vdmVJdGVtKGAke3RoaXMuYXBwTmFtZX1BdXRoQXBwYCksZ2xvYmFsVGhpcy5sb2NhbFN0b3JhZ2UucmVtb3ZlSXRlbShgJHt0aGlzLmFwcE5hbWV9QnVpbHRJbkF1dGhgKSxnbG9iYWxUaGlzLmhpc3RvcnkucHVzaFN0YXRlKHt9LGRvY3VtZW50LnRpdGxlLGdsb2JhbFRoaXMubG9jYXRpb24ucGF0aG5hbWUpLHRoaXMuc2V0U3RhdGUoe2F1dGhlbnRpY2F0aW5nOiExLGVycm9yOnZvaWQgMH0sdm9pZCB0aGlzLmFmdGVyTG9nb3V0KX07aGFuZGxlTG9naW49KHQsZSxzPSExKT0+e2NvbnN0IGE9bmV3IFVSTFNlYXJjaFBhcmFtcyhnbG9iYWxUaGlzLmxvY2F0aW9uLnNlYXJjaCk7aWYodm9pZCAwPT09dCl7Y29uc3QgZT1hLmdldCgiYXV0aEFwcCIpO3Q9bnVsbCE9PWU/ZTp2b2lkIDB9aWYodm9pZCAwPT09ZSl7Y29uc3QgdD1hLmdldCgiYWNjZXNzVG9rZW4iKTtlPW51bGwhPT10P3Q6dm9pZCAwfWlmKHZvaWQgMD09PWUpe2NvbnN0IHQ9Z2xvYmFsVGhpcy5sb2NhbFN0b3JhZ2UuZ2V0SXRlbShgJHt0aGlzLmFwcE5hbWV9Snd0QWNjZXNzVG9rZW5gKTtlPW51bGwhPT10P3Q6dm9pZCAwfWVsc2UgZ2xvYmFsVGhpcy5sb2NhbFN0b3JhZ2Uuc2V0SXRlbShgJHt0aGlzLmFwcE5hbWV9Snd0QWNjZXNzVG9rZW5gLGUpLGdsb2JhbFRoaXMuaGlzdG9yeS5yZXBsYWNlU3RhdGUodm9pZCAwLCIiLHRoaXMuZ2V0VXJsV2l0aE5ld1NlYXJjaFN0cmluZygpKTtpZih2b2lkIDA9PT10KXtjb25zdCBlPWdsb2JhbFRoaXMubG9jYWxTdG9yYWdlLmdldEl0ZW0oYCR7dGhpcy5hcHBOYW1lfUF1dGhBcHBgKTt0PW51bGwhPT1lP2U6dm9pZCAwO2NvbnN0IGE9Z2xvYmFsVGhpcy5sb2NhbFN0b3JhZ2UuZ2V0SXRlbShgJHt0aGlzLmFwcE5hbWV9QnVpbHRJbkF1dGhgKTtzPW51bGwhPT1hJiYidHJ1ZSI9PT1hfWVsc2UgZ2xvYmFsVGhpcy5sb2NhbFN0b3JhZ2Uuc2V0SXRlbShgJHt0aGlzLmFwcE5hbWV9QXV0aEFwcGAsdCksZ2xvYmFsVGhpcy5sb2NhbFN0b3JhZ2Uuc2V0SXRlbShgJHt0aGlzLmFwcE5hbWV9QnVpbHRJbkF1dGhgLFN0cmluZyhzKSk7dGhpcy5zZXNzaW9uJiZlJiYodGhpcy5zZXNzaW9uLmFjY2Vzc1Rva2VuPWUsdGhpcy5zZXNzaW9uLmF1dGhBcHA9dCksdGhpcy5zZXRTdGF0ZSh7YXV0aGVudGljYXRpbmc6dm9pZCAwIT09dGhpcy5zZXNzaW9uPy5hY2Nlc3NUb2tlbiYmIXN9LCgoKT0+e3ZvaWQgMCE9PXRoaXMuc2Vzc2lvbj8uYWNjZXNzVG9rZW4mJnRoaXMuc2Vzc2lvbi5nZXRBdXRoZW50aWNhdGlvblN0YXR1cygpLnRoZW4oKHQ9PnsiYXV0aG9yaXplZCI9PT10Py5zdGF0dXM/dGhpcy5hZnRlckhhbmRsZUxvZ2luKHQpLmNhdGNoKCgoKT0+e3RoaXMubG9nb3V0KCl9KSk6dGhpcy5sb2dvdXQoKX0pKX0pKX07YWZ0ZXJMb2dvdXQ9YXN5bmMoKT0+e307YWZ0ZXJIYW5kbGVMb2dpbj1hc3luYyB0PT57fX1jbGFzcyBNb2RhbEVycm9yIGV4dGVuZHMgQ29tcG9uZW50e3JlbmRlcj0oe2Vycm9yOnQscmVzZXRFcnJvcjplLGxvZ291dDpzfSk9PnQ/aHRtbGAgPGRpdiBjbGFzcz0ibW9kYWwiPiA8ZGl2IGNsYXNzPSJlcnJvciI+IDxwPiR7dC5zdGFjay5yZXBsYWNlKC9hY2Nlc3NUb2tlbj0uKj9bJjpdL2dtLCJhY2Nlc3NUb2tlbj1YOiIpfTwvcD4gPGRpdiBjbGFzcz0iZXJyb3JCdXR0b25zIj4gPGJ1dHRvbiBjbGFzcz0iZmxhdEJ1dHRvbiIgb25DbGljaz0keygpPT5lKCl9PkNsb3NlPC9idXR0b24+IDxidXR0b24gY2xhc3M9ImZsYXRCdXR0b24iIG9uQ2xpY2s9JHsoKT0+cygpfT5SZXN0YXJ0PC9idXR0b24+IDwvZGl2PiA8L2Rpdj4gPC9kaXY+YDoiIn1jbGFzcyBNcnNMb2dpbiBleHRlbmRzIENvbXBvbmVudHtjb25zdHJ1Y3Rvcih0KXtzdXBlcih0KSx0aGlzLnN0YXRlPXt1c2VyTmFtZToiIix1c2VyTmFtZVZlcmlmaWVkOiExLGF1dGhBcHBzOnZvaWQgMCxlcnJvcjp2b2lkIDAsYXV0aEFwcDp0LmF1dGhBcHB9fWNvbXBvbmVudERpZE1vdW50KCl7Y29uc3R7c2Vzc2lvbjp0fT10aGlzLnByb3BzO3QuZ2V0QXV0aEFwcHMoKS50aGVuKCh0PT57dGhpcy5zZXRTdGF0ZSh7YXV0aEFwcHM6dH0pfSkpLmNhdGNoKCh0PT57dGhpcy5zaG93TG9naW5FcnJvcih0KX0pKX1zZXRTdGF0ZUNsYXNzU3R5bGU9KHQsZSk9Pntjb25zdCBzPWRvY3VtZW50LmdldEVsZW1lbnRCeUlkKHQpOyJhY3RpdmUiIT09ZSYmcz8uY2xhc3NMaXN0LnJlbW92ZSgiYWN0aXZlIiksImxvYWRpbmciIT09ZSYmcz8uY2xhc3NMaXN0LnJlbW92ZSgibG9hZGluZyIpLHZvaWQgMCE9PWUmJnM/LmNsYXNzTGlzdC5hZGQoZSl9O3VzZXJOYW1lQ2hhbmdlPXQ9Pnt0aGlzLnNldFN0YXRlQ2xhc3NTdHlsZSgidXNlck5hbWVCdG4iLCIiIT09dD8iYWN0aXZlIjp2b2lkIDApLHRoaXMuc2V0U3RhdGUoe3VzZXJOYW1lOnQsdXNlck5hbWVWZXJpZmllZDohMSxwYXNzd29yZDp2b2lkIDAsbG9naW5FcnJvcjp2b2lkIDB9KX07cGFzc3dvcmRDaGFuZ2U9dD0+e3RoaXMuc2V0U3RhdGVDbGFzc1N0eWxlKCJwYXNzd29yZEJ0biIsIiIhPT10PyJhY3RpdmUiOnZvaWQgMCksdGhpcy5zZXRTdGF0ZSh7cGFzc3dvcmQ6dH0pfTtzaG93TG9naW5FcnJvcj10PT57dGhpcy5zZXRTdGF0ZUNsYXNzU3R5bGUoInVzZXJOYW1lQnRuIiksdGhpcy5zZXRTdGF0ZUNsYXNzU3R5bGUoInBhc3N3b3JkQnRuIiksdGhpcy5zZXRTdGF0ZSh7cGFzc3dvcmQ6dm9pZCAwLGxvZ2luRXJyb3I6dH0sKCgpPT57Y29uc3QgdD1kb2N1bWVudC5nZXRFbGVtZW50QnlJZCgidXNlck5hbWUiKTt0Py5mb2N1cygpLHQ/LnNlbGVjdCgpfSkpfTt2ZXJpZnlVc2VyTmFtZT1hc3luYyB0PT57Y29uc3R7c2Vzc2lvbjplfT10aGlzLnByb3BzLHt1c2VyTmFtZTpzfT10aGlzLnN0YXRlO3RoaXMuc2V0U3RhdGVDbGFzc1N0eWxlKCJ1c2VyTmFtZUJ0biIsImxvYWRpbmciKTt0cnl7YXdhaXQgZS52ZXJpZnlVc2VyTmFtZSh0LHMpLHRoaXMuc2V0U3RhdGUoe3VzZXJOYW1lVmVyaWZpZWQ6ITB9LCgoKT0+e2RvY3VtZW50LmdldEVsZW1lbnRCeUlkKCJ1c2VyUGFzc3dvcmQiKT8uZm9jdXMoKX0pKX1maW5hbGx5e3RoaXMuc2V0U3RhdGVDbGFzc1N0eWxlKCJ1c2VyTmFtZUJ0biIsIiIhPT1zPyJhY3RpdmUiOnZvaWQgMCl9fTt2ZXJpZnlQYXNzd29yZD1hc3luYygpPT57Y29uc3R7aGFuZGxlTG9naW46dCxzZXNzaW9uOmV9PXRoaXMucHJvcHMse3Bhc3N3b3JkOnN9PXRoaXMuc3RhdGU7aWYodm9pZCAwIT09cyYmIiIhPT1zKXt0aGlzLnNldFN0YXRlQ2xhc3NTdHlsZSgicGFzc3dvcmRCdG4iLCJsb2FkaW5nIik7Y29uc3QgYT1hd2FpdCBlLnZlcmlmeVBhc3N3b3JkKHMpO251bGwhPT1hLmVycm9yQ29kZSYmdm9pZCAwIT09YS5lcnJvckNvZGUmJnRoaXMuc2hvd0xvZ2luRXJyb3IoYS5lcnJvck1lc3NhZ2UpLHQoYS5hdXRoQXBwLGEuand0LCEwKSx0aGlzLnNldFN0YXRlQ2xhc3NTdHlsZSgidXNlck5hbWVCdG4iLCIiIT09cz8iYWN0aXZlIjp2b2lkIDApfX07cmVuZGVyPSh0LGUpPT57Y29uc3R7dXNlck5hbWU6cyxwYXNzd29yZDphLGxvZ2luRXJyb3I6aSx1c2VyTmFtZVZlcmlmaWVkOm8sYXV0aEFwcHM6cixhdXRoQXBwOm59PWU7bGV0IGw7aWYobD1uP3I/LmZpbmQoKHQ9PnQubmFtZS50b0xvd2VyQ2FzZSgpPT09bi50b0xvd2VyQ2FzZSgpKSk6cj8uZmluZCgodD0+IjB4MzAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAiPT09dC52ZW5kb3JJZCkpLHZvaWQgMD09PXIpcmV0dXJuIHZvaWQgMCE9PWk/aHRtbGA8YnIgLz4gPGRpdiBjbGFzc05hbWU9Im1yc0xvZ2luRXJyb3IiPiA8cD4ke1N0cmluZyhpKX08L3A+IDwvZGl2PmA6aHRtbGAgPGRpdiBjbGFzc05hbWU9Im1yc0xvZ2luIj4gPHA+TG9hZGluZyAuLi48L3A+IDwvZGl2PmA7aWYoMD09PXIubGVuZ3RoKXJldHVybiBodG1sYCA8YnIgLz4gPGRpdiBjbGFzc05hbWU9Im1yc0xvZ2luRXJyb3IiPiA8cD5ObyBBdXRoZW50aWNhdGlvbiBBcHBzIGRlZmluZWQgZm9yIHRoaXMgUkVTVCBTZXJ2aWNlLjwvcD4gPC9kaXY+YDtpZihuJiYhbClyZXR1cm4gaHRtbGAgPGJyIC8+IDxkaXYgY2xhc3NOYW1lPSJtcnNMb2dpbkVycm9yIj4gPHA+UmVxdWVzdGVkIEF1dGhlbnRpY2F0aW9uIEFwcCAke259IG5vdCBmb3VuZCBvbiBSRVNUIFNlcnZpY2UuPC9wPiA8L2Rpdj5gO2lmKCIweDMwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwIj09PWw/LnZlbmRvcklkKXtjb25zdCB0PWh0bWxgIDxkaXYgaWQ9InVzZXJOYW1lQnRuIiBjbGFzc05hbWU9Im1yc0xvZ2luQnRuTmV4dCIgb25DbGljaz0keygpPT57dGhpcy52ZXJpZnlVc2VyTmFtZShsLm5hbWUpfX0gb25LZXlQcmVzcz0keygpPT57fX0gcm9sZT0iYnV0dG9uIiB0YWJJbmRleD17LTF9IC8+YCxlPWh0bWxgIDxkaXYgY2xhc3NOYW1lPSJtcnNMb2dpbkZpZWxkIj4gPGlucHV0IHR5cGU9InRleHQiIGlkPSJ1c2VyTmFtZSIgcGxhY2Vob2xkZXI9IlVzZXIgTmFtZSBvciBFbWFpbCIgYXV0b0ZvY3VzIHZhbHVlPSR7c30gb25JbnB1dD0ke3Q9Pnt0aGlzLnVzZXJOYW1lQ2hhbmdlKHQudGFyZ2V0LnZhbHVlKX19IG9uS2V5UHJlc3M9JHt0PT57MTM9PT10LmtleUNvZGUmJnRoaXMudmVyaWZ5VXNlck5hbWUobC5uYW1lKX19IC8+ICR7IW8mJnR9IDwvZGl2PmAscj1odG1sYCA8ZGl2IGNsYXNzTmFtZT0ibXJzTG9naW5GaWVsZCI+IDxpbnB1dCB0eXBlPSJwYXNzd29yZCIgaWQ9InVzZXJQYXNzd29yZCIgcGxhY2Vob2xkZXI9IlBhc3N3b3JkIiB2YWx1ZT0ke2F9IG9uSW5wdXQ9JHt0PT57dGhpcy5wYXNzd29yZENoYW5nZSh0LnRhcmdldC52YWx1ZSl9fSBvbktleVByZXNzPSR7dD0+ezEzPT09dC5rZXlDb2RlJiZ0aGlzLnZlcmlmeVBhc3N3b3JkKCl9fSAvPiA8ZGl2IGlkPSJwYXNzd29yZEJ0biIgY2xhc3NOYW1lPSJtcnNMb2dpbkJ0bk5leHQiIG9uQ2xpY2s9JHsoKT0+e3RoaXMudmVyaWZ5UGFzc3dvcmQoKX19IG9uS2V5UHJlc3M9JHsoKT0+e319IHJvbGU9ImJ1dHRvbiIgdGFiSW5kZXg9ey0yfSAvPiA8L2Rpdj5gO3JldHVybiBodG1sYCA8ZGl2IGNsYXNzTmFtZT0ibXJzTG9naW4iPiA8cD48YnIvPjxici8+U2lnbiBpbiB3aXRoICR7bC5uYW1lfTwvcD4gPGRpdiBjbGFzc05hbWU9Im1yc0xvZ2luRmllbGRzIj4gJHtlfSAke28mJnJ9IDwvZGl2PiAke3ZvaWQgMCE9PWkmJmh0bWxgPGRpdiBjbGFzc05hbWU9Im1yc0xvZ2luRXJyb3IiPjxwPiR7aX08L3A+PC9kaXY+YH0gPGRpdiBjbGFzc05hbWU9Im1yc0xvZ2luU2VwYXJhdG9yIiAvPiA8L2Rpdj5gfXJldHVybiBodG1sYCA8ZGl2IGNsYXNzTmFtZT0ibXJzTG9naW4iPiA8cD5SZXF1ZXN0ZWQgQXV0aGVudGljYXRpb24gQXBwICR7bn0gbm90IHN1cHBvcnRlZCB5ZXQuPC9wPiA8L2Rpdj5gfX1jbGFzcyBBcHAgZXh0ZW5kcyBNcnNCYXNlQXBwe2NvbnN0cnVjdG9yKHQpe3N1cGVyKCJNcnNMb2dpbiIsdCl9c3RhcnRMb2dpbj1hc3luYyB0PT57Y29uc3R7c2VydmljZVVybDplLGF1dGhQYXRoOnMscmVkaXJlY3RVcmw6YX09dGhpcy5wcm9wcyxpPWAke2V9JHtzfS9sb2dpbj9hcHA9JHt0fSZzZXNzaW9uVHlwZT1iZWFyZXImb25Db21wbGV0aW9uUmVkaXJlY3Q9JHtlbmNvZGVVUklDb21wb25lbnQodGhpcy5nZXRVcmxXaXRoTmV3U2VhcmNoU3RyaW5nKGBhdXRoQXBwPSR7dH0mcmVkaXJlY3RVcmw9JHthfWApKX1gO3dpbmRvdy5sb2NhdGlvbi5ocmVmPWl9O3Nob3dQYWdlPSh0LGU9ITApPT57d2luZG93Lmhpc3RvcnkucHVzaFN0YXRlKHZvaWQgMCx2b2lkIDAsIiMiK3QpLGUmJnRoaXMuZm9yY2VVcGRhdGUoKX07c2hvd0Vycm9yPXQ9Pnt0aGlzLnNldFN0YXRlKHtlcnJvcjp0fSl9O3JlbmRlcj0oe3NlcnZpY2U6dCxhdXRoQXBwOmV9LHtlcnJvcjpzLGF1dGhlbnRpY2F0aW5nOmEscmVzdGFydGluZzppLGxvZ2luQ29tcGxldGU6b30pPT57d2luZG93LmxvY2F0aW9uLmhhc2g7Y29uc3Qgcj1pPyIiOmh0bWxgPCR7TW9kYWxFcnJvcn0gZXJyb3I9JHtzfSByZXNldEVycm9yPSR7dGhpcy5zaG93RXJyb3J9IGxvZ291dD0ke3RoaXMubG9nb3V0fS8+YCxuPWh0bWxgIDxkaXYgY2xhc3M9IndlbGNvbWVMb2dvIiAvPiA8ZGl2IGNsYXNzPSJ3ZWxjb21lVGV4dCI+IDxoMj5NeVNRTCBSRVNUIFNlcnZpY2U8L2gyPiA8cD5XZWxjb21lIHRvIHRoZSBNeVNRTCBSRVNUIFNlcnZpY2UuPC9wPiA8L2Rpdj5gLGw9aHRtbGAgPGRpdiBjbGFzcz0id2VsY29tZVNwYWNlciI+PC9kaXY+IDxkaXYgY2xhc3M9ImZvb3RlciI+IENvcHlyaWdodCAoYykgMjAyMiwgMjAyNCwgT3JhY2xlIGFuZC9vciBpdHMgYWZmaWxpYXRlcy4gPC9kaXY+IGA7cmV0dXJuIGE/aHRtbGAke3J9IDxkaXYgY2xhc3M9InBhZ2UiPiA8ZGl2IGNsYXNzPSJkb0NlbnRlciI+IDxwPkxvYWRpbmcgLi4uPC9wPiA8L2Rpdj4gPC9kaXY+YDp0JiYhbz9odG1sYCR7cn0ke259IDwke01yc0xvZ2lufSBzdGFydExvZ2luPSR7dGhpcy5zdGFydExvZ2lufSBoYW5kbGVMb2dpbj0ke3RoaXMuaGFuZGxlTG9naW59IHNlc3Npb249JHt0aGlzLnNlc3Npb259IGF1dGhBcHA9JHtlfSAvPiAke2x9YDpvP2h0bWxgJHtufSA8ZGl2IGNsYXNzPSJ3ZWxjb21lVGV4dCI+IDxwPkxvZ2dlZCBpbiBzdWNjZXNzZnVsbHkuPC9wPiA8L2Rpdj4gJHtsfWA6aHRtbGAke259IDxkaXYgY2xhc3M9IndlbGNvbWVUZXh0Ij4gPHA+UGxlYXNlIHVzZSB0aGUgTXlTUUwgU2hlbGwgdG8gY29uZmlndXJlIHlvdXIgTXlTUUwgUkVTVCBTZXJ2aWNlLjwvcD4gPC9kaXY+IDxkaXYgY2xhc3M9IndlbGNvbWVMaW5rcyI+IDxhIGhyZWY9Imh0dHBzOi8vYmxvZ3Mub3JhY2xlLmNvbS9teXNxbC9wb3N0L2ludHJvZHVjaW5nLXRoZS1teXNxbC1yZXN0LXNlcnZpY2UiPkxlYXJuIE1vcmUgPjwvYT4gPGEgaHJlZj0iaHR0cHM6Ly9kZXYubXlzcWwuY29tL2RvYy9teXNxbC1zaGVsbC1ndWkvZW4vbXlzcWwtc2hlbGwtdnNjb2RlLXJlc3Qtc2VydmljZXMuaHRtbCI+QnJvd3NlIFR1dG9yaWFscyA+PC9hPiA8YSBocmVmPSJodHRwczovL2Rldi5teXNxbC5jb20vZG9jL215c3FsLXNoZWxsLWd1aS9lbiI+UmVhZCBEb2NzID48L2E+IDwvZGl2PiAke2x9YH07YWZ0ZXJIYW5kbGVMb2dpbj1hc3luYyB0PT57Y29uc3R7cmVkaXJlY3RVcmw6ZX09dGhpcy5wcm9wcztlP3dpbmRvdy5sb2NhdGlvbi5ocmVmPWU6dGhpcy5zZXRTdGF0ZSh7bG9naW5Db21wbGV0ZTohMH0pfX1jb25zdCB1cmxQYXJhbXM9bmV3IFByb3h5KG5ldyBVUkxTZWFyY2hQYXJhbXMod2luZG93LmxvY2F0aW9uLnNlYXJjaCkse2dldDoodCxlKT0+dC5nZXQoZSl9KTtyZW5kZXIoaHRtbGA8JHtBcHB9IHNlcnZpY2U9JHt1cmxQYXJhbXMuc2VydmljZX0gYXV0aFBhdGg9JHt1cmxQYXJhbXMuYXV0aFBhdGh9IGF1dGhBcHA9JHt1cmxQYXJhbXMuYXV0aEFwcH0gcmVkaXJlY3RVcmw9JHt1cmxQYXJhbXMucmVkaXJlY3RVcmx9Lz5gLGRvY3VtZW50LnF1ZXJ5U2VsZWN0b3IoIiNyb290IikpPC9zY3JpcHQ+PC9ib2R5PjwvaHRtbD4=\", \"favicon.ico\": \"AAABAAMAMDAQAAEABABoBgAANgAAACAgEAABAAQA6AIAAJ4GAAAQEBAAAQAEACgBAACGCQAAKAAAADAAAABgAAAAAQAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIVaCACOZAAAkWsgAJ17LwCoiUMAsJRbALykcADHs4oAz7+ZANrNsgDm3MgA7ujZAPXx6wD++/YA////AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAGcgAAAAAAAAAAAAAAAAAAAAAAAAAAAAK+7CAAAnYAAAAAAAAAAAAAAAAAAAAAABzYzlAAfHAAAAAAAAAAAAAAAAAAAAAAAJ5QvmAq5wAAAAAAAAAAAAAAAAAAAAAABOkAzmCuYAAAAAAAAAAAAAAAAAAAAAAACuMA7ljYAAAAAAAAAAAAAAAAAAAAAAAALZAA3q6gAAAAAAAAAAAAAAAAAAAAAAAAblAAvuwQAAAAAAAAAAAAAAAAAAAAAAAAnhAAnuUAAAAAAAAAAAAAAAAAAAAAAAAAygAATpAAAAAAAAAAAAAAAAAAAAAAAAAB6QAACiAAAAAAAAAAAAAAAAAAAAAAAAAC5wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAC5gAAAAAAAAAAAAAAAAAAAAAAAAAAAAAC5wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAB6AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA6QAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAvAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAngAAAAAAAAAAAAAAAgAAAAAAAAAAAAAG6wAAAAAAAAAAAAAAOgAAAAAAAAAAAABewwAAAAAAAAAAAAADxwAAAAAAAAAAAATrIAAAAAAAAAAAAAAb5AAAAAAAAAAAABvCAAAAAAAAAAAAAACOsAAAAAAAAAAAAH5gAAAAAAAAACVmQgB+UAAAAAAAAAAAA9sAAAAAAAAAa+yqzqQCAAAAAAAAAAAACeUAAAAAAAAqxiAAA36RAAAAAAAAAAAAPqAAAAAAAAPJEAAAAAKrAAAAAAAAAAAArjAAABAAABuABagAqUAakAAAAAAAAAAF6AAAAKIAAHsQXoMAScIC1QAAAAAAAAA8wgAACOIAAuMAbiAABOQAawAAAAAAAAPMUAAAfoAABrAAbgAAA+QAHjAAAAAAADzlAAAAAgAACXAAjgAABOUACmAAAAAABM5QAAAAAAAACmAXyQAAALtgCXAAAAAALeQAAAAAAAAAC1A9owAAAEvQCIAAAAAAjlAAAAFlRDR3CmACrAAAAtgQCXAAAAAAywAAAmze7t7sCJAAfgAABOUADFAAAAAAvDJGne7Jq7uXBMEAbgAAA+QAPhAAAAAAbu7e7sYQAAAAALcAbTAABeMAqAAAAAAABr3LdAAAAAAAAE0wLMcAnKAGwgAAAAAAAAAAAAAAAAAAAAjDAFQAVABdUAAAAAAAAAAAAAAAAAAAAAB+YAAAACjlAAAAAAAAAAAAAAAAAAAAAAAFu4U0WcowAAAAAAAAAAAAAAAAAAAAAAAAFZvduEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAoAAAAIAAAAEAAAAABAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAhVoIAIthAACQah8AmnYnAKCANwCtkFIAuKBsAMCpdwDEsYYAz7+aANfIqgDe07oA597OAPDp2gD///8AAAAAAPAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAFpAAAMAAAAAAAAAAAAAAAfs0AS0AAAAAAAAAAAAAAA+OeBeQAAAAAAAAAAAAAAAuAnV5QAAAAAAAAAAAAAAAuEJ7lAAAAAAAAAAAAAAAAawB+sAAAAAAAAAAAAAAAAJgALiAAAAAAAAAAAAAAAAC1AAIAAAAAAAAAAAAAAAAAtQAAAAAAAAAAAAAAAAAAAKYAAAAAAAAAAAAAAAAAAACoAAAAAAAAAAAAAAAAAAAAigAAAAAAAAABAAAAAAAABNgAAAAAAAAAVwAAAAAAAC2AAAAAAAAAA+QAAAAAAAC6AAAAAAAAAAzAAAAAAAAF4QAAAAA6zMpCQAAAAAAADXAAAAAIxAADqhAAAAAAAG0AAAAAiQVwVhagAAAAAALVAAVgArBNMCtglQAAAAAtoABOMAlQWwAIgCsAAAAC2gAAAQAMAGoABqANAAAAHaAAAAAADAbTAALKCxAAAHwAAEqHijwAigAGoBwAAACLE2ztztxJQFsACIArAAAAPu7sYAAAA7BNMCtglQAAAAJUEAAAAACJBnBXBqAAAAAAAAAAAAAACLQAA5oQAAAAAAAAAAAAAABJzMtQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAoAAAAEAAAACAAAAABAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAhVoIAIxiAACQayAAmnYhAJ59MgCnhz4Aq5BRALGVXQC3nGYAuqJvAMGqeADHs4wA0cKeANrQtgDv7uIAAAAAAAAAAAAAAAAAAAAALVBQAAAAAADHvBAAAAAAArXiAAAAAAAHQGAAAAAAAAcwAAAAAAAABVAAAAAAAAAMIAAABjAAAKQAAGg7AAACsAAqQWgAAAwRoJKQtEAAwwAAiUCzgAWTvdmEcLGAAbtgAGSSeRAAAAAAB4mTAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==\", \"favicon.svg\": \"PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRGLTgiIHN0YW5kYWxvbmU9Im5vIj8+CjwhRE9DVFlQRSBzdmcgUFVCTElDICItLy9XM0MvL0RURCBTVkcgMS4xLy9FTiIgImh0dHA6Ly93d3cudzMub3JnL0dyYXBoaWNzL1NWRy8xLjEvRFREL3N2ZzExLmR0ZCI+Cjxzdmcgd2lkdGg9IjEwMCUiIGhlaWdodD0iMTAwJSIgdmlld0JveD0iMCAwIDE2IDE2IiB2ZXJzaW9uPSIxLjEiIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgeG1sbnM6eGxpbms9Imh0dHA6Ly93d3cudzMub3JnLzE5OTkveGxpbmsiIHhtbDpzcGFjZT0icHJlc2VydmUiIHhtbG5zOnNlcmlmPSJodHRwOi8vd3d3LnNlcmlmLmNvbS8iIHN0eWxlPSJmaWxsLXJ1bGU6ZXZlbm9kZDtjbGlwLXJ1bGU6ZXZlbm9kZDtzdHJva2UtbGluZWpvaW46cm91bmQ7c3Ryb2tlLW1pdGVybGltaXQ6MjsiPgogICAgPGcgdHJhbnNmb3JtPSJtYXRyaXgoMS4wMDE0OSwwLDAsMS4wMDE0OSwtMC4wNDkxNjYzLC0wLjA2MTg4KSI+CiAgICAgICAgPHBhdGggZD0iTTE2LjAyNSwxLjYzNkMxNi4wMjUsMC43NjcgMTUuMzIsMC4wNjIgMTQuNDUxLDAuMDYyTDEuNjI0LDAuMDYyQzAuNzU1LDAuMDYyIDAuMDQ5LDAuNzY3IDAuMDQ5LDEuNjM2TDAuMDQ5LDE0LjQ2M0MwLjA0OSwxNS4zMzIgMC43NTUsMTYuMDM4IDEuNjI0LDE2LjAzOEwxNC40NTEsMTYuMDM4QzE1LjMyLDE2LjAzOCAxNi4wMjUsMTUuMzMyIDE2LjAyNSwxNC40NjNMMTYuMDI1LDEuNjM2WiIgc3R5bGU9ImZpbGw6cmdiKDAsOTAsMTMzKTsiLz4KICAgIDwvZz4KICAgIDxnIGlkPSJtcnMiIHRyYW5zZm9ybT0ibWF0cml4KDAuMTA5OTUsLTAuMDE4MTI4NiwwLjAxODIyOTksMC4xMTA1NjQsMi45MjIsMC43NTM4NikiPgogICAgICAgIDxnIGlkPSJzYWtpbGFab29tZWQiIHRyYW5zZm9ybT0ibWF0cml4KDEuNDM1OTMsLTIuMjcwM2UtMTYsLTIuNzc1NTZlLTE3LDEuNDM1OTMsLTQwLjYyNTIsNTYuOTkzNykiPgogICAgICAgICAgICA8cGF0aCBkPSJNNTMuNzE0LC0xNS41ODlDNTIuMDYzLC0xNi4zNjEgNTAuMzM3LC0xNy4wNDMgNDguNTMsLTE3LjU5MkM0Ni4xODQsLTE4LjMwNiA0My4zNzEsLTE3LjkxMSA0MC44NTgsLTE4LjQ4OUwzOS4yNTMsLTE4LjQ4OUMzNy44NDYsLTE4LjkwMiAzNi42NywtMjAuMzk1IDM1LjUwNCwtMjEuMTY1QzMzLjA4MSwtMjIuNzc2IDMwLjY5NCwtMjMuOTA2IDI3LjgzMywtMjUuMDk2QzI2Ljc1NCwtMjUuNTQyIDIzLjg3MiwtMjYuNjEgMjIuODM2LC0yNS44MUMyMi4yMywtMjUuNjExIDIxLjk1OCwtMjUuMzUzIDIxLjc2NiwtMjQuNzQyQzIxLjE2MSwtMjMuODI4IDIxLjcxNiwtMjIuNDA5IDIyLjEyMiwtMjEuNTI0QzIzLjI3LC0xOS4wMzMgMjQuOTI2LC0xNy41MTEgMjYuNDA2LC0xNS40NDdDMjcuNzQyLC0xMy41ODggMjkuMzY5LC0xMS40NzEgMzAuMzMsLTkuMzc0QzMyLjM0MywtNC45OTggMzMuMjM2LC0wLjEzMyAzNS4xNDgsNC4xOTlDMzUuODgzLDUuODYzIDM2Ljk0Myw3Ljc2MyAzOC4wMDQsOS4xOTlDMzguODYxLDEwLjM3IDQwLjM5NCwxMS4yNDcgNDAuODU4LDEyLjc3NkM0MS44MywxNC4zMDEgMzkuNDI4LDE5LjUxOCAzOC44OTUsMjEuMTc0QzM2LjgzOSwyNy41NjQgMzcuMjc0LDM2LjUxMiAzOS42MDksNDIuMDcxQzQwLjUzMyw0NC4yNyA0MS40MDcsNDYuODMxIDQzLjg5Miw0Ny40M0M0NC4wNzUsNDcuMjg4IDQzLjkzNSw0Ny4zNjUgNDQuMjQ4LDQ3LjI1MUM0NC43NzgsNDIuOTczIDQ0Ljk2MiwzOC44NDkgNDYuMzksMzUuNDY0QzQ3LjI4MywzMy4zNDMgNDguOTgzLDMxLjg5NiA1MC4xMzgsMzAuMTAxQzUwLjk4MywzMC41OTUgNTAuOTc5LDMyLjAxOCA1MS4zODUsMzIuOTZDNTIuNDQxLDM1LjQwNyA1My41MjcsMzguMDYzIDU0Ljc3Niw0MC40NjRDNTcuMzk1LDQ1LjQ5NyA2Mi43MzQsNTEuNjk4IDYzLjI0Nyw1Mi4zODJDNjUuMjA5LDU0Ljk5NiA2MS43MjUsNTIuOTY3IDU4Ljk2OSw1MC45NjdDNTYuODA3LDQ5LjM5OSA1NS44NTksNDguMzgyIDU0LjE2NCw0Ni4yNTVDNTIuNjcyLDQ0LjM4MyA1MS41NjQsNDEuNzEyIDUwLjQ5NCwzOS41NjdMNTAuNDk0LDM5LjM5MkM1MC4wNDYsMzkuOTkxIDUwLjE4Niw0MC42NDQgNDkuOTU4LDQxLjUzMkM0OC45NDksNDUuNDY1IDQ5LjczNiw0OS45MjMgNDYuMjEyLDUxLjM2M0M0Mi4xOCw1My4wMDUgMzkuMjQ5LDQ4LjcxNiAzOC4wMDQsNDYuNzE3QzMzLjk0Nyw0MC4xOTkgMzIuODk1LDI5LjI2MSAzNS42ODUsMjAuNDZDMzYuMzAyLDE4LjUwMiAzNi4zNjQsMTYuMTA4IDM3LjQ2OCwxNC41NjJDMzcuMjg5LDEzLjE3NSAzNi4xNjIsMTIuNzggMzUuNTA0LDExLjg4M0MzNC40MzMsMTAuNDE4IDMzLjQ5OSw4LjY5MyAzMi42NSw3LjA1N0MzMC45NzMsMy44MjQgMjkuODQ1LDAuMDE4IDI4LjU0NywtMy40ODFDMjguMDE5LC00LjkgMjcuOTEzLC02LjI0NyAyNy4yOTgsLTcuNTkyQzI2LjM2LC05LjYzMSAyNC42ODIsLTExLjY2NyAyMy4zNzEsLTEzLjQ4NkMyMS41MDQsLTE2LjA3MSAxNi4yODMsLTIxLjA2IDE4LjM3NiwtMjYuMTdDMjEuNjkxLC0zNC4yNjUgMzMuMTcxLC0yOC4xMDcgMzcuNjQ3LC0yNS4yNzdDMzguNzcsLTI0LjU2MyA0MC4wMTcsLTIzLjA5NSA0MS4yMTUsLTIyLjU5N0M0My4xNzcsLTIyLjQ3NCA0NC41NDQsLTIyLjcwMSA0Ny4xMDMsLTIyLjIzN0M1MC4wNDUsLTIxLjcwNSA1Mi44MTEsLTIxLjA0OSA1NS4yNjYsLTE5Ljk1OEw1My43MTQsLTE1LjU4OVpNODcuNjYxLDEwLjY4NEM4OS4yMDYsMTMuNTg5IDkwLjA5NCwxOC45NzkgOTAuMTIsMjEuMDFDOTAuMTQ2LDIzLjEyIDg5LjUxNywyMy4yMjcgODguODE1LDIyLjI5Qzg2LjUwNCwxOS4yMDYgODUuMTQzLDE2LjMzNiA4NC4yMTgsMTQuNTYyQzgzLjg5LDEzLjkzMyA4My41NTQsMTMuMzA5IDgzLjIwOSwxMi42OTJMODcuNjYxLDEwLjY4NFpNNDQuMDcsLTEwLjgwNkM0NC45NzEsLTEwLjYwOCA0NS43LC05Ljc1OCA0Ni4yMTIsLTkuMDE5QzQ2LjU5NSwtOC40NjUgNDYuNzQ2LC04LjA0OSA0Ni44NDcsLTcuMDY4QzQ3LjA0NCwtNS4xNzcgNDYuNjAzLC00LjI2MSA0NS40OTgsLTMuNDgxQzQ1LjQzOCwtMy40MjQgNDUuMzc5LC0zLjM2MyA0NS4zMiwtMy4zMDZDNDQuNzI0LC00LjU1NCA0NC4yNDksLTUuODcgNDMuNTM1LC03LjA1NEM0Mi4yNTEsLTkuMTgzIDQyLjA4NywtOC44MTYgNDEuMDM2LC0xMC4yNzFDNDEuMDAyLC0xMC4zMiA0MC45MTgsLTEwLjI3MSA0MC44NTgsLTEwLjI3MUw0MC44NTgsLTEwLjQ0N0M0MS44MzgsLTEwLjY2MyA0Mi43NjIsLTEwLjgzIDQ0LjA3LC0xMC44MDZaIiBzdHlsZT0iZmlsbDp3aGl0ZTsiLz4KICAgICAgICA8L2c+CiAgICAgICAgPGcgdHJhbnNmb3JtPSJtYXRyaXgoMC4wNTYyNjgsMC4wMDkyMjU5OCwtMC4wMDkyNzc1NCwwLjA1NTk1NTYsNDUuMDIzOSw5LjQ5MDExKSI+CiAgICAgICAgICAgIDxwYXRoIGQ9Ik01MTEuNzE1LDEuNTk1Qzc5My45NzYsMS41OTUgMTAyMy4xMywyMzAuNzU1IDEwMjMuMTMsNTEzLjAxNkMxMDIzLjEzLDc5NS4yNzcgNzkzLjk3NiwxMDI0LjQ0IDUxMS43MTUsMTAyNC40NEMyMjkuNDU0LDEwMjQuNDQgMC4yOTQsNzk1LjI3NyAwLjI5NCw1MTMuMDE2QzAuMjk0LDIzMC43NTUgMjI5LjQ1NCwxLjU5NSA1MTEuNzE1LDEuNTk1Wk01MTEuNzE1LDY1LjE5QzI2NC41NTMsNjUuMTkgNjMuODg5LDI2NS44NTQgNjMuODg5LDUxMy4wMTZDNjMuODg5LDc2MC4xNzggMjY0LjU1Myw5NjAuODQyIDUxMS43MTUsOTYwLjg0MkM3NTguODc3LDk2MC44NDIgOTU5LjU0MSw3NjAuMTc4IDk1OS41NDEsNTEzLjAxNkM5NTkuNTQxLDI2NS44NTQgNzU4Ljg3Nyw2NS4xOSA1MTEuNzE1LDY1LjE5WiIgc3R5bGU9ImZpbGw6d2hpdGU7Ii8+CiAgICAgICAgPC9nPgogICAgICAgIDxnIHRyYW5zZm9ybT0ibWF0cml4KDMuNjAxMTUsMC41OTA0NjMsLTAuNTkzNzYyLDMuNTgxMTYsNTQuMjYzNSwtMC40NDE5ODYpIj4KICAgICAgICAgICAgPGcgdHJhbnNmb3JtPSJtYXRyaXgoMS4xODU3OCwwLDAsMS4wNDIyNSwtMC43ODEwMDUsLTAuNDY2NTAzKSI+CiAgICAgICAgICAgICAgICA8cGF0aCBkPSJNNC41NjEsMTUuOTE2TDQuODc4LDE1LjkxNkw0Ljg3OCwxNS4wNkw0LjY2MywxNS4wNkM0LjMxOCwxNS4wNiA0LjA3LDE0Ljk3NSAzLjkyLDE0LjgwNkMzLjc2OSwxNC42MzcgMy42OTQsMTQuMzU2IDMuNjk0LDEzLjk2MkwzLjY5NCwxMi4zOTlDMy42OTQsMTIuMDMgMy41OTMsMTEuNzQzIDMuMzkyLDExLjU0QzMuMTksMTEuMzM3IDIuODkxLDExLjIxNCAyLjQ5NCwxMS4xNzNMMi40OTQsMTEuMDM1QzIuODkxLDEwLjk5NCAzLjE5LDEwLjg3MSAzLjM5MiwxMC42NjZDMy41OTMsMTAuNDYxIDMuNjk0LDEwLjE3NCAzLjY5NCw5LjgwNEwzLjY5NCw4LjI2MUMzLjY5NCw3Ljg2OCAzLjc2OSw3LjU4NyAzLjkyLDcuNDE4QzQuMDcsNy4yNDkgNC4zMTgsNy4xNjQgNC42NjMsNy4xNjRMNC44NzgsNy4xNjRMNC44NzgsNi4zMDhMNC41NjEsNi4zMDhDMy44OTcsNi4zMDggMy40MTEsNi40NTUgMy4xMDIsNi43NDlDMi43OTMsNy4wNDMgMi42MzgsNy41MDIgMi42MzgsOC4xMjhMMi42MzgsOS40NTZDMi42MzgsOS44NTYgMi41NTIsMTAuMTM1IDIuMzgyLDEwLjI5NEMyLjIxMSwxMC40NTMgMS45MSwxMC41MzMgMS40NzksMTAuNTMzTDEuNDc5LDExLjY3MUMxLjkxLDExLjY3NCAyLjIxMSwxMS43NTUgMi4zODIsMTEuOTE0QzIuNTUyLDEyLjA3MyAyLjYzOCwxMi4zNTEgMi42MzgsMTIuNzQ3TDIuNjM4LDE0LjA5MUMyLjYzOCwxNC43MTYgMi43OTMsMTUuMTc3IDMuMTAyLDE1LjQ3MkMzLjQxMSwxNS43NjggMy44OTcsMTUuOTE2IDQuNTYxLDE1LjkxNloiIHN0eWxlPSJmaWxsOndoaXRlO2ZpbGwtcnVsZTpub256ZXJvOyIvPgogICAgICAgICAgICA8L2c+CiAgICAgICAgICAgIDxnIHRyYW5zZm9ybT0ibWF0cml4KDEuMTc5MDMsMCwwLDEuMDQyMjUsLTEuMzcxOTYsLTAuNDY2NTAzKSI+CiAgICAgICAgICAgICAgICA8cGF0aCBkPSJNNy4zMzQsMTUuOTE2QzcuOTk3LDE1LjkxNiA4LjQ4MywxNS43NjggOC43OTMsMTUuNDcyQzkuMTAyLDE1LjE3NyA5LjI1NywxNC43MTYgOS4yNTcsMTQuMDkxTDkuMjU3LDEyLjc0N0M5LjI1NywxMi4zNTEgOS4zNDIsMTIuMDczIDkuNTEzLDExLjkxNEM5LjY4NCwxMS43NTUgOS45ODUsMTEuNjc0IDEwLjQxNSwxMS42NzFMMTAuNDE1LDEwLjUzM0M5Ljk4NSwxMC41MzMgOS42ODQsMTAuNDUzIDkuNTEzLDEwLjI5NEM5LjM0MiwxMC4xMzUgOS4yNTcsOS44NTYgOS4yNTcsOS40NTZMOS4yNTcsOC4xMjhDOS4yNTcsNy41MDIgOS4xMDIsNy4wNDMgOC43OTMsNi43NDlDOC40ODMsNi40NTUgNy45OTcsNi4zMDggNy4zMzQsNi4zMDhMNy4wMTYsNi4zMDhMNy4wMTYsNy4xNjRMNy4yMzIsNy4xNjRDNy41OCw3LjE2NCA3LjgyOSw3LjI0OSA3Ljk3OCw3LjQxOEM4LjEyNiw3LjU4NyA4LjIwMSw3Ljg2OCA4LjIwMSw4LjI2MUw4LjIwMSw5LjgwNEM4LjIwMSwxMC4xNzQgOC4zMDEsMTAuNDYxIDguNTAzLDEwLjY2NkM4LjcwNSwxMC44NzEgOS4wMDYsMTAuOTk0IDkuNDA1LDExLjAzNUw5LjQwNSwxMS4xNzNDOS4wMDYsMTEuMjE0IDguNzA1LDExLjMzNyA4LjUwMywxMS41NEM4LjMwMSwxMS43NDMgOC4yMDEsMTIuMDMgOC4yMDEsMTIuMzk5TDguMjAxLDEzLjk2MkM4LjIwMSwxNC4zNTYgOC4xMjUsMTQuNjM3IDcuOTc1LDE0LjgwNkM3LjgyNSwxNC45NzUgNy41NzcsMTUuMDYgNy4yMzIsMTUuMDZMNy4wMTYsMTUuMDZMNy4wMTYsMTUuOTE2TDcuMzM0LDE1LjkxNloiIHN0eWxlPSJmaWxsOndoaXRlO2ZpbGwtcnVsZTpub256ZXJvOyIvPgogICAgICAgICAgICA8L2c+CiAgICAgICAgPC9nPgogICAgPC9nPgo8L3N2Zz4K\", \"sakila.svg\": \"PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRGLTgiIHN0YW5kYWxvbmU9Im5vIj8+CjwhRE9DVFlQRSBzdmcgUFVCTElDICItLy9XM0MvL0RURCBTVkcgMS4xLy9FTiIgImh0dHA6Ly93d3cudzMub3JnL0dyYXBoaWNzL1NWRy8xLjEvRFREL3N2ZzExLmR0ZCI+Cjxzdmcgd2lkdGg9IjEwMCUiIGhlaWdodD0iMTAwJSIgdmlld0JveD0iMCAwIDEyOCAxMjgiIHZlcnNpb249IjEuMSIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIiB4bWxuczp4bGluaz0iaHR0cDovL3d3dy53My5vcmcvMTk5OS94bGluayIgeG1sOnNwYWNlPSJwcmVzZXJ2ZSIgeG1sbnM6c2VyaWY9Imh0dHA6Ly93d3cuc2VyaWYuY29tLyIgc3R5bGU9ImZpbGwtcnVsZTpldmVub2RkO2NsaXAtcnVsZTpldmVub2RkO3N0cm9rZS1saW5lam9pbjpyb3VuZDtzdHJva2UtbWl0ZXJsaW1pdDoyOyI+CiAgICA8ZyB0cmFuc2Zvcm09Im1hdHJpeCgyLjEzMzMzLDAsMCwyLjEzMzMzLC03Ni43OTk5LC04OC44Mzk0KSI+CiAgICAgICAgPHBhdGggZD0iTTkxLjQ4Nyw4Ny4xNzVDOTEuNDg3LDg3LjE3NSA4NS41MSw4Ny40MjYgODMuMzQ2LDg4LjEwMUM4Mi43Miw4OC4yOTcgODEuNTI4LDg4LjQ1MyA4MS42NjcsODkuMTg1QzgxLjcyNiw4OS40OTMgODIuMDUzLDkwLjAyOSA4Mi4zMTksOTAuNDQ2QzgyLjgyOCw5MS4yNDUgODMuNjkxLDkyLjMxNyA4NC40NTksOTIuODc4Qzg1LjMsOTMuNDk0IDg2LjE2NCw5NC4xNSA4Ny4wNjUsOTQuNjgzQzg4LjY2Niw5NS42MjkgOTAuNDU1LDk2LjE2OCA5MS45OTcsOTcuMTE1QzkyLjkwNiw5Ny42NzIgOTMuODA5LDk4LjM3OCA5NC42OTYsOTkuMDA4Qzk1LjEzMyw5OS4zMTggOTUuNDM2LDk5Ljc3NCA5NS45OTksOTkuOTk5Qzk2LjAyNywxMDAuMDExIDk1LjYxNSw5OS4wMzcgOTUuMzQ3LDk4LjY0NUM5NC45NjUsOTguMDg4IDk0LjU4NSw5Ny41OTMgOTQuMTc1LDk3LjIwOUM5Mi41ODMsOTUuNzIxIDkxLjQ1Myw5NC42MTkgODkuODU2LDkzLjUxQzg4LjU4Myw5Mi42MjUgODUuNDY2LDkwLjk5NiA4NS4xMSw4OS45MDVDODQuOTY0LDg5LjQ1NiA4Ny4wMzEsODkuMzkgODcuOTAyLDg5LjI3NUM4OS4zMzYsODkuMDg3IDkwLjcwMSw4OS4wNTcgOTIuMTQ2LDg4LjY5N0M5Mi43OTgsODguNTE2IDk0LjEyMSw4Ny45NTkgOTQuMDY0LDg3Ljg1MkM5My41NjYsODYuOTAyIDkzLjE3LDg2LjI0NCA5MS45OTMsODUuMjQxQzg5LjkzOCw4My40ODggODcuNTQ1LDgxLjk2NSA4NS4yMDMsODAuNTMzQzgzLjkwNSw3OS43MzkgODIuMjk5LDc5LjIyMiA4MC45MjMsNzguNTQ5QzgwLjQ2LDc4LjMyMyA3OS42NDYsNzguMjA2IDc5LjM0LDc3LjgyOUM3OC42MTcsNzYuOTM0IDc4LjIyMyw3NS44MDUgNzcuNjY2LDc0Ljc2M0M3Ni40OTcsNzIuNTg3IDc1Ljc2OCw2OC45MjEgNzIuMjY4LDYzLjQxQzY4LjAwOSw1Ni43MDMgNjMuNDk0LDUyLjYyIDU2LjQ0OCw0OC42MjhDNTQuOTQ5LDQ3Ljc4MSA1My4xODYsNDcuMjU5IDUxLjIzNyw0Ny4wMDdDNTAuMjIsNDYuODc1IDQ5LjE4OSw0Ni44ODggNDguMTY1LDQ2LjgyNkM0Ny41NDEsNDYuNTc1IDQ2Ljg5LDQ1LjgzNCA0Ni4zMDQsNDUuNDc0QzQzLjk3LDQ0LjA0NiAzNy45ODMsNDAuOTM5IDM2LjI1NCw0NS4wMjNDMzUuMTYzLDQ3LjYwMSAzNy44ODUsNTAuMTE4IDM4Ljg1OSw1MS40MjJDMzkuNTQzLDUyLjM0IDQwLjQxOCw1My4zNjcgNDAuOTA3LDU0LjM5NkM0MS4yMjgsNTUuMDc1IDQxLjI4Myw1NS43NTQgNDEuNTU4LDU2LjQ3QzQyLjIzNSw1OC4yMzUgNDIuODI0LDYwLjE1NSA0My42OTgsNjEuNzg2QzQ0LjE0MSw2Mi42MTIgNDQuNDEzLDYzLjE0NyA0NC45NzIsNjMuODg1QzQ1LjMxNSw2NC4zMzggNDYuMTM0LDY0Ljk5NCA0Ni4xNTIsNjUuNTM3QzQ2LjE4NCw2Ni41MDUgNDUuNjAzLDY3LjU2IDQ1LjI4MSw2OC41NDhDNDMuODI2LDcyLjk4OCA0NC4zNzQsNzguNTA2IDQ2LjQ5LDgxLjc5NEM0Ny4xNCw4Mi44MDIgNDguNjY5LDg0Ljk2NyA1MC43NzEsODQuMTM4QzUyLjYxLDgzLjQxMSA1Mi4xOTksODEuMTYzIDUyLjcyNSw3OS4xNzlDNTIuODQ0LDc4LjczIDUyLjc3MSw3OC40MDEgNTMuMDA1LDc4LjA5OUw1My4wMDUsNzguMTg3QzUzLjU2Myw3OS4yNyA1NC4xMjEsODAuMzUyIDU0LjY4LDgxLjQzMkM1NS45Miw4My4zNjYgNTcuOTY1LDg1LjU2NSA1OS44MDgsODcuMDg2QzYwLjczMiw4Ny44NDggNjMuMDM3LDg5LjI5OSA2My4wMTUsODkuMjc3QzYyLjY2OSw4OC45MjIgNjIuMjQ4LDg4LjQ4NyA2MS45MzksODguMTkzQzYxLjIxLDg3LjUwMiA2MC40LDg2LjY0MiA1OS43OTgsODUuODQ5QzU4LjEwMiw4My42MTkgNTYuNjA0LDgxLjE3OSA1NS4yMzgsNzguNjRDNTQuNTg2LDc3LjQyOCA1NC4xMTcsNzYuMDA1IDUzLjY2OSw3NC45ODNDNTMuNDYsNzQuNTA2IDUzLjIwNyw3My4wODYgNTIuODE4LDczLjQxMkM1MS44NjcsNzQuMjEgNTEuNjEzLDc1LjEyNCA1MS4xNTUsNzYuMTk4QzUwLjYzNCw3Ny40MjIgNTAuODg3LDgwLjA3NiA0OS45ODYsODEuOTI1QzQ5LjkxLDgyLjA4MSA0OS42NzEsODIuMjAzIDQ5LjU2MSw4Mi4xNTRDNDkuMDQzLDgxLjkyNiA0OC4yMzYsODAuNTc3IDQ3Ljc5Nyw3OS40NTFDNDYuNDA3LDc1Ljg4OCA0Ni4xNDgsNzMuMzcxIDQ3LjQyNCw2OC45MDhDNDcuNjY3LDY4LjA2MSA0OC41MjMsNjUuMTExIDQ4LjAxNiw2NC4zNDJDNDcuNzc0LDYzLjU3IDQ2LjkzOCw2Mi45ODkgNDYuNDksNjIuMzk4QzQ1LjkzNyw2MS42NzQgNDUuMzg0LDYwLjcxNSA0NS4wMDEsNTkuODc1QzQ0LjAwNCw1Ny42OSA0My41MzgsNTUuMjM1IDQyLjQ4OSw1My4wMjhDNDEuOTg3LDUxLjk3IDQxLjEzOCw1MC45MDIgNDAuNDQyLDQ5Ljk2NEMzOS42Nyw0OC45MjMgMzguODA2LDQ4LjE1NSAzOC4yMDgsNDYuODk4QzM3Ljk5NSw0Ni40NTIgMzcuNzk1LDQ2LjI1NCAzOC4wMjIsNDUuNzQzQzM4LjE0Niw0NS40NjYgMzguMzExLDQ1LjMzNSAzOC41OCw0NS4yMDRDMzkuNDA3LDQ0LjgwMyA0MC42MjMsNDUuMzQgNDEuMTg2LDQ1LjU2NUM0Mi42NzgsNDYuMTY1IDQzLjkyMyw0Ni43MzUgNDUuMTg3LDQ3LjU0OEM0NS43OTUsNDcuOTM3IDQ2LjQwOCw0OC42OSA0Ny4xNDIsNDguODk4QzQ3LjQyMSw0OC44OTggNDcuNyw0OC44OTMgNDcuOTc5LDQ4Ljg5OEM0OS40MTEsNDguOTI1IDUwLjc1Nyw0OC45OSA1MS45ODEsNDkuMzVDNTQuMTQ0LDQ5Ljk4NiA1Ni4wODMsNTAuOTc4IDU3Ljg0NCw1Mi4wNTJDNjMuMjA4LDU1LjMzNCA2Ny4yMTcsNTkuMTkxIDcwLjM3Myw2NC42NzdDNzMuMzQ4LDY5Ljg1IDc0LjI2MSw3My4wMTggNzUuMTE1LDc0Ljg5NEM3NS45NjYsNzYuNzY2IDc2LjE4Myw3Ny44MDUgNzcuMzg2LDc5LjM2MkM3OC4wMTksODAuMTc5IDgwLjQ2NCw4MC42MTkgODEuNTc0LDgxLjA3MkM4Mi4zNTMsODEuMzkxIDgzLjY3NSw4MS44MjIgODQuNDEzLDgyLjI1MkM4NS44MjIsODMuMDczIDg3LjEzOSw4My45NTcgODguNDYxLDg0Ljg1OEM4OS4xMjEsODUuMzA5IDkwLjUzNyw4Ni4zNTUgOTEuNDg3LDg3LjE3NVoiIHN0eWxlPSJmaWxsOnJnYigwLDkwLDEzMyk7Ii8+CiAgICA8L2c+CiAgICA8ZyB0cmFuc2Zvcm09Im1hdHJpeCgyLjEzMzMzLDAsMCwyLjEzMzMzLC03Ni43OTk5LC04OC44Mzk0KSI+CiAgICAgICAgPHBhdGggZD0iTTQ5LjY1NSw1Mi43NzRDNDkuMDA2LDUyLjUwMiA0OC40OSw1Mi44NDYgNDcuOTc5LDUyLjk1NUM0Ny45NzksNTIuOTg2IDQ4LjY2Myw1My40MzQgNDkuMzc1LDU0LjY2N0M1MC4wNDMsNTUuODI0IDQ5Ljk5Niw1NS45MjggNTAuMzA2LDU2LjU1OEM1MC4zMzcsNTYuNTI5IDUxLjQxMiw1NS41MzggNTEuMTY0LDU0LjYxMkM1MS4wNTMsNTQuMjAxIDUwLjk3MSw1My45NTUgNTAuNzcxLDUzLjY3NkM1MC41MDQsNTMuMzAzIDUwLjA5NCw1Mi45NTkgNDkuNjU1LDUyLjc3NFoiIHN0eWxlPSJmaWxsOnJnYigwLDkwLDEzMyk7Ii8+CiAgICA8L2c+Cjwvc3ZnPgo=\", \"standalone-preact.js\": \"Ly8gcHJlYWN0QDEwLjIzLjEgKGgsIHJlbmRlciwgQ29tcG9uZW50LCBjcmVhdGVSZWYpLCBDb3B5cmlnaHQgKGMpIDIwMjQsIE9yYWNsZSBhbmQvb3IgaXRzIGFmZmlsaWF0ZXMuCnZhciBILHYsWSx1ZSx4LEosWixSLEIsJCxBLGZlLE09e30sZWU9W10sY2U9L2FjaXR8ZXgoPzpzfGd8bnxwfCQpfHJwaHxncmlkfG93c3xtbmN8bnR3fGluZVtjaF18em9vfF5vcmR8aXRlcmEvaSxqPUFycmF5LmlzQXJyYXk7ZnVuY3Rpb24gdyhfLGUpe2Zvcih2YXIgdCBpbiBlKV9bdF09ZVt0XTtyZXR1cm4gX31mdW5jdGlvbiBfZShfKXt2YXIgZT1fLnBhcmVudE5vZGU7ZSYmZS5yZW1vdmVDaGlsZChfKX1mdW5jdGlvbiBWKF8sZSx0KXt2YXIgbyxpLG4sbD17fTtmb3IobiBpbiBlKW49PSJrZXkiP289ZVtuXTpuPT0icmVmIj9pPWVbbl06bFtuXT1lW25dO2lmKGFyZ3VtZW50cy5sZW5ndGg+MiYmKGwuY2hpbGRyZW49YXJndW1lbnRzLmxlbmd0aD4zP0guY2FsbChhcmd1bWVudHMsMik6dCksdHlwZW9mIF89PSJmdW5jdGlvbiImJl8uZGVmYXVsdFByb3BzIT1udWxsKWZvcihuIGluIF8uZGVmYXVsdFByb3BzKWxbbl09PT12b2lkIDAmJihsW25dPV8uZGVmYXVsdFByb3BzW25dKTtyZXR1cm4gVyhfLGwsbyxpLG51bGwpfWZ1bmN0aW9uIFcoXyxlLHQsbyxpKXt2YXIgbj17dHlwZTpfLHByb3BzOmUsa2V5OnQscmVmOm8sX19rOm51bGwsX186bnVsbCxfX2I6MCxfX2U6bnVsbCxfX2Q6dm9pZCAwLF9fYzpudWxsLGNvbnN0cnVjdG9yOnZvaWQgMCxfX3Y6aT8/KytZLF9faTotMSxfX3U6MH07cmV0dXJuIGk9PW51bGwmJnYudm5vZGUhPW51bGwmJnYudm5vZGUobiksbn1mdW5jdGlvbiBwZSgpe3JldHVybntjdXJyZW50Om51bGx9fWZ1bmN0aW9uIEkoXyl7cmV0dXJuIF8uY2hpbGRyZW59ZnVuY3Rpb24gVShfLGUpe3RoaXMucHJvcHM9Xyx0aGlzLmNvbnRleHQ9ZX1mdW5jdGlvbiBDKF8sZSl7aWYoZT09bnVsbClyZXR1cm4gXy5fXz9DKF8uX18sXy5fX2krMSk6bnVsbDtmb3IodmFyIHQ7ZTxfLl9fay5sZW5ndGg7ZSsrKWlmKCh0PV8uX19rW2VdKSE9bnVsbCYmdC5fX2UhPW51bGwpcmV0dXJuIHQuX19lO3JldHVybiB0eXBlb2YgXy50eXBlPT0iZnVuY3Rpb24iP0MoXyk6bnVsbH1mdW5jdGlvbiB0ZShfKXt2YXIgZSx0O2lmKChfPV8uX18pIT1udWxsJiZfLl9fYyE9bnVsbCl7Zm9yKF8uX19lPV8uX19jLmJhc2U9bnVsbCxlPTA7ZTxfLl9fay5sZW5ndGg7ZSsrKWlmKCh0PV8uX19rW2VdKSE9bnVsbCYmdC5fX2UhPW51bGwpe18uX19lPV8uX19jLmJhc2U9dC5fX2U7YnJlYWt9cmV0dXJuIHRlKF8pfX1mdW5jdGlvbiBLKF8peyghXy5fX2QmJihfLl9fZD0hMCkmJngucHVzaChfKSYmIUYuX19yKyt8fEohPT12LmRlYm91bmNlUmVuZGVyaW5nKSYmKChKPXYuZGVib3VuY2VSZW5kZXJpbmcpfHxaKShGKX1mdW5jdGlvbiBGKCl7dmFyIF8sZSx0LG8saSxuLGwscztmb3IoeC5zb3J0KFIpO189eC5zaGlmdCgpOylfLl9fZCYmKGU9eC5sZW5ndGgsbz12b2lkIDAsbj0oaT0odD1fKS5fX3YpLl9fZSxsPVtdLHM9W10sdC5fX1AmJigobz13KHt9LGkpKS5fX3Y9aS5fX3YrMSx2LnZub2RlJiZ2LnZub2RlKG8pLHoodC5fX1AsbyxpLHQuX19uLHQuX19QLm5hbWVzcGFjZVVSSSwzMiZpLl9fdT9bbl06bnVsbCxsLG4/P0MoaSksISEoMzImaS5fX3UpLHMpLG8uX192PWkuX192LG8uX18uX19rW28uX19pXT1vLHJlKGwsbyxzKSxvLl9fZSE9biYmdGUobykpLHgubGVuZ3RoPmUmJnguc29ydChSKSk7Ri5fX3I9MH1mdW5jdGlvbiBuZShfLGUsdCxvLGksbixsLHMsZix1LHApe3ZhciByLGEsYyx5LGcsbT1vJiZvLl9fa3x8ZWUsZD1lLmxlbmd0aDtmb3IodC5fX2Q9ZixhZSh0LGUsbSksZj10Ll9fZCxyPTA7cjxkO3IrKykoYz10Ll9fa1tyXSkhPW51bGwmJnR5cGVvZiBjIT0iYm9vbGVhbiImJnR5cGVvZiBjIT0iZnVuY3Rpb24iJiYoYT1jLl9faT09PS0xP006bVtjLl9faV18fE0sYy5fX2k9cix6KF8sYyxhLGksbixsLHMsZix1LHApLHk9Yy5fX2UsYy5yZWYmJmEucmVmIT1jLnJlZiYmKGEucmVmJiZxKGEucmVmLG51bGwsYykscC5wdXNoKGMucmVmLGMuX19jfHx5LGMpKSxnPT1udWxsJiZ5IT1udWxsJiYoZz15KSw2NTUzNiZjLl9fdXx8YS5fX2s9PT1jLl9faz9mPW9lKGMsZixfKTp0eXBlb2YgYy50eXBlPT0iZnVuY3Rpb24iJiZjLl9fZCE9PXZvaWQgMD9mPWMuX19kOnkmJihmPXkubmV4dFNpYmxpbmcpLGMuX19kPXZvaWQgMCxjLl9fdSY9LTE5NjYwOSk7dC5fX2Q9Zix0Ll9fZT1nfWZ1bmN0aW9uIGFlKF8sZSx0KXt2YXIgbyxpLG4sbCxzLGY9ZS5sZW5ndGgsdT10Lmxlbmd0aCxwPXUscj0wO2ZvcihfLl9faz1bXSxvPTA7bzxmO28rKylsPW8rciwoaT1fLl9fa1tvXT0oaT1lW29dKT09bnVsbHx8dHlwZW9mIGk9PSJib29sZWFuInx8dHlwZW9mIGk9PSJmdW5jdGlvbiI/bnVsbDp0eXBlb2YgaT09InN0cmluZyJ8fHR5cGVvZiBpPT0ibnVtYmVyInx8dHlwZW9mIGk9PSJiaWdpbnQifHxpLmNvbnN0cnVjdG9yPT1TdHJpbmc/VyhudWxsLGksbnVsbCxudWxsLG51bGwpOmooaSk/VyhJLHtjaGlsZHJlbjppfSxudWxsLG51bGwsbnVsbCk6aS5jb25zdHJ1Y3Rvcj09PXZvaWQgMCYmaS5fX2I+MD9XKGkudHlwZSxpLnByb3BzLGkua2V5LGkucmVmP2kucmVmOm51bGwsaS5fX3YpOmkpIT1udWxsPyhpLl9fPV8saS5fX2I9Xy5fX2IrMSxzPWRlKGksdCxsLHApLGkuX19pPXMsbj1udWxsLHMhPT0tMSYmKHAtLSwobj10W3NdKSYmKG4uX191fD0xMzEwNzIpKSxuPT1udWxsfHxuLl9fdj09PW51bGw/KHM9PS0xJiZyLS0sdHlwZW9mIGkudHlwZSE9ImZ1bmN0aW9uIiYmKGkuX191fD02NTUzNikpOnMhPT1sJiYocz09bC0xP3I9cy1sOnM9PWwrMT9yKys6cz5sP3A+Zi1sP3IrPXMtbDpyLS06czxsJiZyKysscyE9PW8rciYmKGkuX191fD02NTUzNikpKToobj10W2xdKSYmbi5rZXk9PW51bGwmJm4uX19lJiYhKDEzMTA3MiZuLl9fdSkmJihuLl9fZT09Xy5fX2QmJihfLl9fZD1DKG4pKSxPKG4sbiwhMSksdFtsXT1udWxsLHAtLSk7aWYocClmb3Iobz0wO288dTtvKyspKG49dFtvXSkhPW51bGwmJiEoMTMxMDcyJm4uX191KSYmKG4uX19lPT1fLl9fZCYmKF8uX19kPUMobikpLE8obixuKSl9ZnVuY3Rpb24gb2UoXyxlLHQpe3ZhciBvLGk7aWYodHlwZW9mIF8udHlwZT09ImZ1bmN0aW9uIil7Zm9yKG89Xy5fX2ssaT0wO28mJmk8by5sZW5ndGg7aSsrKW9baV0mJihvW2ldLl9fPV8sZT1vZShvW2ldLGUsdCkpO3JldHVybiBlfV8uX19lIT1lJiYoZSYmXy50eXBlJiYhdC5jb250YWlucyhlKSYmKGU9QyhfKSksdC5pbnNlcnRCZWZvcmUoXy5fX2UsZXx8bnVsbCksZT1fLl9fZSk7ZG8gZT1lJiZlLm5leHRTaWJsaW5nO3doaWxlKGUhPW51bGwmJmUubm9kZVR5cGU9PT04KTtyZXR1cm4gZX1mdW5jdGlvbiBkZShfLGUsdCxvKXt2YXIgaT1fLmtleSxuPV8udHlwZSxsPXQtMSxzPXQrMSxmPWVbdF07aWYoZj09PW51bGx8fGYmJmk9PWYua2V5JiZuPT09Zi50eXBlJiYhKDEzMTA3MiZmLl9fdSkpcmV0dXJuIHQ7aWYobz4oZiE9bnVsbCYmISgxMzEwNzImZi5fX3UpPzE6MCkpZm9yKDtsPj0wfHxzPGUubGVuZ3RoOyl7aWYobD49MCl7aWYoKGY9ZVtsXSkmJiEoMTMxMDcyJmYuX191KSYmaT09Zi5rZXkmJm49PT1mLnR5cGUpcmV0dXJuIGw7bC0tfWlmKHM8ZS5sZW5ndGgpe2lmKChmPWVbc10pJiYhKDEzMTA3MiZmLl9fdSkmJmk9PWYua2V5JiZuPT09Zi50eXBlKXJldHVybiBzO3MrK319cmV0dXJuLTF9ZnVuY3Rpb24gUShfLGUsdCl7ZVswXT09PSItIj9fLnNldFByb3BlcnR5KGUsdD8/IiIpOl9bZV09dD09bnVsbD8iIjp0eXBlb2YgdCE9Im51bWJlciJ8fGNlLnRlc3QoZSk/dDp0KyJweCJ9ZnVuY3Rpb24gTChfLGUsdCxvLGkpe3ZhciBuO2U6aWYoZT09PSJzdHlsZSIpaWYodHlwZW9mIHQ9PSJzdHJpbmciKV8uc3R5bGUuY3NzVGV4dD10O2Vsc2V7aWYodHlwZW9mIG89PSJzdHJpbmciJiYoXy5zdHlsZS5jc3NUZXh0PW89IiIpLG8pZm9yKGUgaW4gbyl0JiZlIGluIHR8fFEoXy5zdHlsZSxlLCIiKTtpZih0KWZvcihlIGluIHQpbyYmdFtlXT09PW9bZV18fFEoXy5zdHlsZSxlLHRbZV0pfWVsc2UgaWYoZVswXT09PSJvIiYmZVsxXT09PSJuIiluPWUhPT0oZT1lLnJlcGxhY2UoLyhQb2ludGVyQ2FwdHVyZSkkfENhcHR1cmUkL2ksIiQxIikpLGU9ZS50b0xvd2VyQ2FzZSgpaW4gX3x8ZT09PSJvbkZvY3VzT3V0Inx8ZT09PSJvbkZvY3VzSW4iP2UudG9Mb3dlckNhc2UoKS5zbGljZSgyKTplLnNsaWNlKDIpLF8ubHx8KF8ubD17fSksXy5sW2Urbl09dCx0P28/dC51PW8udToodC51PUIsXy5hZGRFdmVudExpc3RlbmVyKGUsbj9BOiQsbikpOl8ucmVtb3ZlRXZlbnRMaXN0ZW5lcihlLG4/QTokLG4pO2Vsc2V7aWYoaT09Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIillPWUucmVwbGFjZSgveGxpbmsoSHw6aCkvLCJoIikucmVwbGFjZSgvc05hbWUkLywicyIpO2Vsc2UgaWYoZSE9IndpZHRoIiYmZSE9ImhlaWdodCImJmUhPSJocmVmIiYmZSE9Imxpc3QiJiZlIT0iZm9ybSImJmUhPSJ0YWJJbmRleCImJmUhPSJkb3dubG9hZCImJmUhPSJyb3dTcGFuIiYmZSE9ImNvbFNwYW4iJiZlIT0icm9sZSImJmUhPSJwb3BvdmVyIiYmZSBpbiBfKXRyeXtfW2VdPXQ/PyIiO2JyZWFrIGV9Y2F0Y2h7fXR5cGVvZiB0PT0iZnVuY3Rpb24ifHwodD09bnVsbHx8dD09PSExJiZlWzRdIT09Ii0iP18ucmVtb3ZlQXR0cmlidXRlKGUpOl8uc2V0QXR0cmlidXRlKGUsZT09InBvcG92ZXIiJiZ0PT0xPyIiOnQpKX19ZnVuY3Rpb24gWChfKXtyZXR1cm4gZnVuY3Rpb24oZSl7aWYodGhpcy5sKXt2YXIgdD10aGlzLmxbZS50eXBlK19dO2lmKGUudD09bnVsbCllLnQ9QisrO2Vsc2UgaWYoZS50PHQudSlyZXR1cm47cmV0dXJuIHQodi5ldmVudD92LmV2ZW50KGUpOmUpfX19ZnVuY3Rpb24geihfLGUsdCxvLGksbixsLHMsZix1KXt2YXIgcCxyLGEsYyx5LGcsbSxkLGgsUCxiLFQsUyxHLEQsTixrPWUudHlwZTtpZihlLmNvbnN0cnVjdG9yIT09dm9pZCAwKXJldHVybiBudWxsOzEyOCZ0Ll9fdSYmKGY9ISEoMzImdC5fX3UpLG49W3M9ZS5fX2U9dC5fX2VdKSwocD12Ll9fYikmJnAoZSk7ZTppZih0eXBlb2Ygaz09ImZ1bmN0aW9uIil0cnl7aWYoZD1lLnByb3BzLGg9InByb3RvdHlwZSJpbiBrJiZrLnByb3RvdHlwZS5yZW5kZXIsUD0ocD1rLmNvbnRleHRUeXBlKSYmb1twLl9fY10sYj1wP1A/UC5wcm9wcy52YWx1ZTpwLl9fOm8sdC5fX2M/bT0ocj1lLl9fYz10Ll9fYykuX189ci5fX0U6KGg/ZS5fX2M9cj1uZXcgayhkLGIpOihlLl9fYz1yPW5ldyBVKGQsYiksci5jb25zdHJ1Y3Rvcj1rLHIucmVuZGVyPXZlKSxQJiZQLnN1YihyKSxyLnByb3BzPWQsci5zdGF0ZXx8KHIuc3RhdGU9e30pLHIuY29udGV4dD1iLHIuX19uPW8sYT1yLl9fZD0hMCxyLl9faD1bXSxyLl9zYj1bXSksaCYmci5fX3M9PW51bGwmJihyLl9fcz1yLnN0YXRlKSxoJiZrLmdldERlcml2ZWRTdGF0ZUZyb21Qcm9wcyE9bnVsbCYmKHIuX19zPT1yLnN0YXRlJiYoci5fX3M9dyh7fSxyLl9fcykpLHcoci5fX3Msay5nZXREZXJpdmVkU3RhdGVGcm9tUHJvcHMoZCxyLl9fcykpKSxjPXIucHJvcHMseT1yLnN0YXRlLHIuX192PWUsYSloJiZrLmdldERlcml2ZWRTdGF0ZUZyb21Qcm9wcz09bnVsbCYmci5jb21wb25lbnRXaWxsTW91bnQhPW51bGwmJnIuY29tcG9uZW50V2lsbE1vdW50KCksaCYmci5jb21wb25lbnREaWRNb3VudCE9bnVsbCYmci5fX2gucHVzaChyLmNvbXBvbmVudERpZE1vdW50KTtlbHNle2lmKGgmJmsuZ2V0RGVyaXZlZFN0YXRlRnJvbVByb3BzPT1udWxsJiZkIT09YyYmci5jb21wb25lbnRXaWxsUmVjZWl2ZVByb3BzIT1udWxsJiZyLmNvbXBvbmVudFdpbGxSZWNlaXZlUHJvcHMoZCxiKSwhci5fX2UmJihyLnNob3VsZENvbXBvbmVudFVwZGF0ZSE9bnVsbCYmci5zaG91bGRDb21wb25lbnRVcGRhdGUoZCxyLl9fcyxiKT09PSExfHxlLl9fdj09PXQuX192KSl7Zm9yKGUuX192IT09dC5fX3YmJihyLnByb3BzPWQsci5zdGF0ZT1yLl9fcyxyLl9fZD0hMSksZS5fX2U9dC5fX2UsZS5fX2s9dC5fX2ssZS5fX2suZm9yRWFjaChmdW5jdGlvbihFKXtFJiYoRS5fXz1lKX0pLFQ9MDtUPHIuX3NiLmxlbmd0aDtUKyspci5fX2gucHVzaChyLl9zYltUXSk7ci5fc2I9W10sci5fX2gubGVuZ3RoJiZsLnB1c2gocik7YnJlYWsgZX1yLmNvbXBvbmVudFdpbGxVcGRhdGUhPW51bGwmJnIuY29tcG9uZW50V2lsbFVwZGF0ZShkLHIuX19zLGIpLGgmJnIuY29tcG9uZW50RGlkVXBkYXRlIT1udWxsJiZyLl9faC5wdXNoKGZ1bmN0aW9uKCl7ci5jb21wb25lbnREaWRVcGRhdGUoYyx5LGcpfSl9aWYoci5jb250ZXh0PWIsci5wcm9wcz1kLHIuX19QPV8sci5fX2U9ITEsUz12Ll9fcixHPTAsaCl7Zm9yKHIuc3RhdGU9ci5fX3Msci5fX2Q9ITEsUyYmUyhlKSxwPXIucmVuZGVyKHIucHJvcHMsci5zdGF0ZSxyLmNvbnRleHQpLEQ9MDtEPHIuX3NiLmxlbmd0aDtEKyspci5fX2gucHVzaChyLl9zYltEXSk7ci5fc2I9W119ZWxzZSBkbyByLl9fZD0hMSxTJiZTKGUpLHA9ci5yZW5kZXIoci5wcm9wcyxyLnN0YXRlLHIuY29udGV4dCksci5zdGF0ZT1yLl9fczt3aGlsZShyLl9fZCYmKytHPDI1KTtyLnN0YXRlPXIuX19zLHIuZ2V0Q2hpbGRDb250ZXh0IT1udWxsJiYobz13KHcoe30sbyksci5nZXRDaGlsZENvbnRleHQoKSkpLGgmJiFhJiZyLmdldFNuYXBzaG90QmVmb3JlVXBkYXRlIT1udWxsJiYoZz1yLmdldFNuYXBzaG90QmVmb3JlVXBkYXRlKGMseSkpLG5lKF8saihOPXAhPW51bGwmJnAudHlwZT09PUkmJnAua2V5PT1udWxsP3AucHJvcHMuY2hpbGRyZW46cCk/TjpbTl0sZSx0LG8saSxuLGwscyxmLHUpLHIuYmFzZT1lLl9fZSxlLl9fdSY9LTE2MSxyLl9faC5sZW5ndGgmJmwucHVzaChyKSxtJiYoci5fX0U9ci5fXz1udWxsKX1jYXRjaChFKXtpZihlLl9fdj1udWxsLGZ8fG4hPW51bGwpe2ZvcihlLl9fdXw9Zj8xNjA6MzI7cyYmcy5ub2RlVHlwZT09PTgmJnMubmV4dFNpYmxpbmc7KXM9cy5uZXh0U2libGluZztuW24uaW5kZXhPZihzKV09bnVsbCxlLl9fZT1zfWVsc2UgZS5fX2U9dC5fX2UsZS5fX2s9dC5fX2s7di5fX2UoRSxlLHQpfWVsc2Ugbj09bnVsbCYmZS5fX3Y9PT10Ll9fdj8oZS5fX2s9dC5fX2ssZS5fX2U9dC5fX2UpOmUuX19lPWhlKHQuX19lLGUsdCxvLGksbixsLGYsdSk7KHA9di5kaWZmZWQpJiZwKGUpfWZ1bmN0aW9uIHJlKF8sZSx0KXtlLl9fZD12b2lkIDA7Zm9yKHZhciBvPTA7bzx0Lmxlbmd0aDtvKyspcSh0W29dLHRbKytvXSx0Wysrb10pO3YuX19jJiZ2Ll9fYyhlLF8pLF8uc29tZShmdW5jdGlvbihpKXt0cnl7Xz1pLl9faCxpLl9faD1bXSxfLnNvbWUoZnVuY3Rpb24obil7bi5jYWxsKGkpfSl9Y2F0Y2gobil7di5fX2UobixpLl9fdil9fSl9ZnVuY3Rpb24gaGUoXyxlLHQsbyxpLG4sbCxzLGYpe3ZhciB1LHAscixhLGMseSxnLG09dC5wcm9wcyxkPWUucHJvcHMsaD1lLnR5cGU7aWYoaD09PSJzdmciP2k9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIjpoPT09Im1hdGgiP2k9Imh0dHA6Ly93d3cudzMub3JnLzE5OTgvTWF0aC9NYXRoTUwiOml8fChpPSJodHRwOi8vd3d3LnczLm9yZy8xOTk5L3hodG1sIiksbiE9bnVsbCl7Zm9yKHU9MDt1PG4ubGVuZ3RoO3UrKylpZigoYz1uW3VdKSYmInNldEF0dHJpYnV0ZSJpbiBjPT0hIWgmJihoP2MubG9jYWxOYW1lPT09aDpjLm5vZGVUeXBlPT09Mykpe189YyxuW3VdPW51bGw7YnJlYWt9fWlmKF89PW51bGwpe2lmKGg9PT1udWxsKXJldHVybiBkb2N1bWVudC5jcmVhdGVUZXh0Tm9kZShkKTtfPWRvY3VtZW50LmNyZWF0ZUVsZW1lbnROUyhpLGgsZC5pcyYmZCksbj1udWxsLHM9ITF9aWYoaD09PW51bGwpbT09PWR8fHMmJl8uZGF0YT09PWR8fChfLmRhdGE9ZCk7ZWxzZXtpZihuPW4mJkguY2FsbChfLmNoaWxkTm9kZXMpLG09dC5wcm9wc3x8TSwhcyYmbiE9bnVsbClmb3IobT17fSx1PTA7dTxfLmF0dHJpYnV0ZXMubGVuZ3RoO3UrKyltWyhjPV8uYXR0cmlidXRlc1t1XSkubmFtZV09Yy52YWx1ZTtmb3IodSBpbiBtKWlmKGM9bVt1XSx1IT0iY2hpbGRyZW4iKXtpZih1PT0iZGFuZ2Vyb3VzbHlTZXRJbm5lckhUTUwiKXI9YztlbHNlIGlmKHUhPT0ia2V5IiYmISh1IGluIGQpKXtpZih1PT0idmFsdWUiJiYiZGVmYXVsdFZhbHVlImluIGR8fHU9PSJjaGVja2VkIiYmImRlZmF1bHRDaGVja2VkImluIGQpY29udGludWU7TChfLHUsbnVsbCxjLGkpfX1mb3IodSBpbiBkKWM9ZFt1XSx1PT0iY2hpbGRyZW4iP2E9Yzp1PT0iZGFuZ2Vyb3VzbHlTZXRJbm5lckhUTUwiP3A9Yzp1PT0idmFsdWUiP3k9Yzp1PT0iY2hlY2tlZCI/Zz1jOnU9PT0ia2V5Inx8cyYmdHlwZW9mIGMhPSJmdW5jdGlvbiJ8fG1bdV09PT1jfHxMKF8sdSxjLG1bdV0saSk7aWYocClzfHxyJiYocC5fX2h0bWw9PT1yLl9faHRtbHx8cC5fX2h0bWw9PT1fLmlubmVySFRNTCl8fChfLmlubmVySFRNTD1wLl9faHRtbCksZS5fX2s9W107ZWxzZSBpZihyJiYoXy5pbm5lckhUTUw9IiIpLG5lKF8saihhKT9hOlthXSxlLHQsbyxoPT09ImZvcmVpZ25PYmplY3QiPyJodHRwOi8vd3d3LnczLm9yZy8xOTk5L3hodG1sIjppLG4sbCxuP25bMF06dC5fX2smJkModCwwKSxzLGYpLG4hPW51bGwpZm9yKHU9bi5sZW5ndGg7dS0tOyluW3VdIT1udWxsJiZfZShuW3VdKTtzfHwodT0idmFsdWUiLHkhPT12b2lkIDAmJih5IT09X1t1XXx8aD09PSJwcm9ncmVzcyImJiF5fHxoPT09Im9wdGlvbiImJnkhPT1tW3VdKSYmTChfLHUseSxtW3VdLGkpLHU9ImNoZWNrZWQiLGchPT12b2lkIDAmJmchPT1fW3VdJiZMKF8sdSxnLG1bdV0saSkpfXJldHVybiBffWZ1bmN0aW9uIHEoXyxlLHQpe3RyeXtpZih0eXBlb2YgXz09ImZ1bmN0aW9uIil7dmFyIG89dHlwZW9mIF8uX191PT0iZnVuY3Rpb24iO28mJl8uX191KCksbyYmZT09bnVsbHx8KF8uX191PV8oZSkpfWVsc2UgXy5jdXJyZW50PWV9Y2F0Y2goaSl7di5fX2UoaSx0KX19ZnVuY3Rpb24gTyhfLGUsdCl7dmFyIG8saTtpZih2LnVubW91bnQmJnYudW5tb3VudChfKSwobz1fLnJlZikmJihvLmN1cnJlbnQmJm8uY3VycmVudCE9PV8uX19lfHxxKG8sbnVsbCxlKSksKG89Xy5fX2MpIT1udWxsKXtpZihvLmNvbXBvbmVudFdpbGxVbm1vdW50KXRyeXtvLmNvbXBvbmVudFdpbGxVbm1vdW50KCl9Y2F0Y2gobil7di5fX2UobixlKX1vLmJhc2U9by5fX1A9bnVsbH1pZihvPV8uX19rKWZvcihpPTA7aTxvLmxlbmd0aDtpKyspb1tpXSYmTyhvW2ldLGUsdHx8dHlwZW9mIF8udHlwZSE9ImZ1bmN0aW9uIik7dHx8Xy5fX2U9PW51bGx8fF9lKF8uX19lKSxfLl9fYz1fLl9fPV8uX19lPV8uX19kPXZvaWQgMH1mdW5jdGlvbiB2ZShfLGUsdCl7cmV0dXJuIHRoaXMuY29uc3RydWN0b3IoXyx0KX1mdW5jdGlvbiB5ZShfLGUsdCl7dmFyIG8saSxuLGw7di5fXyYmdi5fXyhfLGUpLGk9KG89dHlwZW9mIHQ9PSJmdW5jdGlvbiIpP251bGw6dCYmdC5fX2t8fGUuX19rLG49W10sbD1bXSx6KGUsXz0oIW8mJnR8fGUpLl9faz1WKEksbnVsbCxbX10pLGl8fE0sTSxlLm5hbWVzcGFjZVVSSSwhbyYmdD9bdF06aT9udWxsOmUuZmlyc3RDaGlsZD9ILmNhbGwoZS5jaGlsZE5vZGVzKTpudWxsLG4sIW8mJnQ/dDppP2kuX19lOmUuZmlyc3RDaGlsZCxvLGwpLHJlKG4sXyxsKX1IPWVlLnNsaWNlLHY9e19fZTpmdW5jdGlvbihfLGUsdCxvKXtmb3IodmFyIGksbixsO2U9ZS5fXzspaWYoKGk9ZS5fX2MpJiYhaS5fXyl0cnl7aWYoKG49aS5jb25zdHJ1Y3RvcikmJm4uZ2V0RGVyaXZlZFN0YXRlRnJvbUVycm9yIT1udWxsJiYoaS5zZXRTdGF0ZShuLmdldERlcml2ZWRTdGF0ZUZyb21FcnJvcihfKSksbD1pLl9fZCksaS5jb21wb25lbnREaWRDYXRjaCE9bnVsbCYmKGkuY29tcG9uZW50RGlkQ2F0Y2goXyxvfHx7fSksbD1pLl9fZCksbClyZXR1cm4gaS5fX0U9aX1jYXRjaChzKXtfPXN9dGhyb3cgX319LFk9MCx1ZT1mdW5jdGlvbihfKXtyZXR1cm4gXyE9bnVsbCYmXy5jb25zdHJ1Y3Rvcj09bnVsbH0sVS5wcm90b3R5cGUuc2V0U3RhdGU9ZnVuY3Rpb24oXyxlKXt2YXIgdDt0PXRoaXMuX19zIT1udWxsJiZ0aGlzLl9fcyE9PXRoaXMuc3RhdGU/dGhpcy5fX3M6dGhpcy5fX3M9dyh7fSx0aGlzLnN0YXRlKSx0eXBlb2YgXz09ImZ1bmN0aW9uIiYmKF89Xyh3KHt9LHQpLHRoaXMucHJvcHMpKSxfJiZ3KHQsXyksXyE9bnVsbCYmdGhpcy5fX3YmJihlJiZ0aGlzLl9zYi5wdXNoKGUpLEsodGhpcykpfSxVLnByb3RvdHlwZS5mb3JjZVVwZGF0ZT1mdW5jdGlvbihfKXt0aGlzLl9fdiYmKHRoaXMuX19lPSEwLF8mJnRoaXMuX19oLnB1c2goXyksSyh0aGlzKSl9LFUucHJvdG90eXBlLnJlbmRlcj1JLHg9W10sWj10eXBlb2YgUHJvbWlzZT09ImZ1bmN0aW9uIj9Qcm9taXNlLnByb3RvdHlwZS50aGVuLmJpbmQoUHJvbWlzZS5yZXNvbHZlKCkpOnNldFRpbWVvdXQsUj1mdW5jdGlvbihfLGUpe3JldHVybiBfLl9fdi5fX2ItZS5fX3YuX19ifSxGLl9fcj0wLEI9MCwkPVgoITEpLEE9WCghMCksZmU9MDt2YXIgbGU9ZnVuY3Rpb24oXyxlLHQsbyl7dmFyIGk7ZVswXT0wO2Zvcih2YXIgbj0xO248ZS5sZW5ndGg7bisrKXt2YXIgbD1lW24rK10scz1lW25dPyhlWzBdfD1sPzE6Mix0W2VbbisrXV0pOmVbKytuXTtsPT09Mz9vWzBdPXM6bD09PTQ/b1sxXT1PYmplY3QuYXNzaWduKG9bMV18fHt9LHMpOmw9PT01PyhvWzFdPW9bMV18fHt9KVtlWysrbl1dPXM6bD09PTY/b1sxXVtlWysrbl1dKz1zKyIiOmw/KGk9Xy5hcHBseShzLGxlKF8scyx0LFsiIixudWxsXSkpLG8ucHVzaChpKSxzWzBdP2VbMF18PTI6KGVbbi0yXT0wLGVbbl09aSkpOm8ucHVzaChzKX1yZXR1cm4gb30saWU9bmV3IE1hcDtmdW5jdGlvbiBzZShfKXt2YXIgZT1pZS5nZXQodGhpcyk7cmV0dXJuIGV8fChlPW5ldyBNYXAsaWUuc2V0KHRoaXMsZSkpLChlPWxlKHRoaXMsZS5nZXQoXyl8fChlLnNldChfLGU9ZnVuY3Rpb24odCl7Zm9yKHZhciBvLGksbj0xLGw9IiIscz0iIixmPVswXSx1PWZ1bmN0aW9uKGEpe249PT0xJiYoYXx8KGw9bC5yZXBsYWNlKC9eXHMqXG5ccyp8XHMqXG5ccyokL2csIiIpKSk/Zi5wdXNoKDAsYSxsKTpuPT09MyYmKGF8fGwpPyhmLnB1c2goMyxhLGwpLG49Mik6bj09PTImJmw9PT0iLi4uIiYmYT9mLnB1c2goNCxhLDApOm49PT0yJiZsJiYhYT9mLnB1c2goNSwwLCEwLGwpOm4+PTUmJigobHx8IWEmJm49PT01KSYmKGYucHVzaChuLDAsbCxpKSxuPTYpLGEmJihmLnB1c2gobixhLDAsaSksbj02KSksbD0iIn0scD0wO3A8dC5sZW5ndGg7cCsrKXtwJiYobj09PTEmJnUoKSx1KHApKTtmb3IodmFyIHI9MDtyPHRbcF0ubGVuZ3RoO3IrKylvPXRbcF1bcl0sbj09PTE/bz09PSI8Ij8odSgpLGY9W2ZdLG49Myk6bCs9bzpuPT09ND9sPT09Ii0tIiYmbz09PSI+Ij8obj0xLGw9IiIpOmw9bytsWzBdOnM/bz09PXM/cz0iIjpsKz1vOm89PT0nIid8fG89PT0iJyI/cz1vOm89PT0iPiI/KHUoKSxuPTEpOm4mJihvPT09Ij0iPyhuPTUsaT1sLGw9IiIpOm89PT0iLyImJihuPDV8fHRbcF1bcisxXT09PSI+Iik/KHUoKSxuPT09MyYmKGY9ZlswXSksbj1mLChmPWZbMF0pLnB1c2goMiwwLG4pLG49MCk6bz09PSIgInx8bz09PSIJInx8bz09PWAKYHx8bz09PSJcciI/KHUoKSxuPTIpOmwrPW8pLG49PT0zJiZsPT09IiEtLSImJihuPTQsZj1mWzBdKX1yZXR1cm4gdSgpLGZ9KF8pKSxlKSxhcmd1bWVudHMsW10pKS5sZW5ndGg+MT9lOmVbMF19dmFyIHdlPXNlLmJpbmQoVik7ZXhwb3J0e1UgYXMgQ29tcG9uZW50LHBlIGFzIGNyZWF0ZVJlZixWIGFzIGgsc2UgYXMgaHRtLHdlIGFzIGh0bWwseWUgYXMgcmVuZGVyfTsK\" }, \"directoryIndexDirective\": [ \"index.html\" ] }');

COMMIT;


# -----------------------------------------------------
# Data for table `mysql_rest_service_metadata`.`mrs_role`
# -----------------------------------------------------
START TRANSACTION;
USE `mysql_rest_service_metadata`;
INSERT INTO `mysql_rest_service_metadata`.`mrs_role` (`id`, `derived_from_role_id`, `specific_to_service_id`, `caption`, `description`, `options`) VALUES (0x31, NULL, NULL, 'Full Access', 'Full access to all db_objects', NULL);

COMMIT;


# -----------------------------------------------------
# Data for table `mysql_rest_service_metadata`.`mrs_user_hierarchy_type`
# -----------------------------------------------------
START TRANSACTION;
USE `mysql_rest_service_metadata`;
INSERT INTO `mysql_rest_service_metadata`.`mrs_user_hierarchy_type` (`id`, `caption`, `description`, `specific_to_service_id`, `options`) VALUES (0x31, 'Direct Report', 'And employee directly reporting to the user', NULL, NULL);
INSERT INTO `mysql_rest_service_metadata`.`mrs_user_hierarchy_type` (`id`, `caption`, `description`, `specific_to_service_id`, `options`) VALUES (0x32, 'Dotted Line Report', 'And employee reporting to the user via a dotted line relationship', NULL, NULL);

COMMIT;


# -----------------------------------------------------
# Data for table `mysql_rest_service_metadata`.`mrs_privilege`
# -----------------------------------------------------
START TRANSACTION;
USE `mysql_rest_service_metadata`;
INSERT INTO `mysql_rest_service_metadata`.`mrs_privilege` (`id`, `role_id`, `crud_operations`, `service_id`, `db_schema_id`, `db_object_id`, `service_path`, `schema_path`, `object_path`, `options`) VALUES (0x31, 0x31, 'CREATE,READ,UPDATE,DELETE', NULL, NULL, NULL, NULL, NULL, NULL, NULL);

COMMIT;

# -----------------------------------------------------
# Additional SQL

# Ensure only one row in `mysql_rest_service_metadata`.`config`
ALTER TABLE `mysql_rest_service_metadata`.`config`
    ADD CONSTRAINT Config_OnlyOneRow CHECK (id = 1);

# Ensure there is a default for service.name taken from url_context_root
ALTER TABLE `mysql_rest_service_metadata`.`service`
    CHANGE COLUMN name name VARCHAR(255) NOT NULL DEFAULT (REGEXP_REPLACE(url_context_root, '[^0-9a-zA-Z ]', ''));

# Ensure page size is within 16K limit
ALTER TABLE `mysql_rest_service_metadata`.`db_schema`
    ADD CONSTRAINT db_schema_max_page_size CHECK (items_per_page IS NULL OR items_per_page < 16384);
ALTER TABLE `mysql_rest_service_metadata`.`db_object`
    ADD CONSTRAINT db_object_max_page_size CHECK (items_per_page IS NULL OR items_per_page < 16384);

DELIMITER $$;

CREATE FUNCTION `mysql_rest_service_metadata`.`get_sequence_id`() RETURNS BINARY(16) SQL SECURITY INVOKER NOT DETERMINISTIC NO SQL
RETURN UUID_TO_BIN(UUID(), 1)$$

CREATE EVENT `mysql_rest_service_metadata`.`delete_old_audit_log_entries` ON SCHEDULE EVERY 1 DAY DISABLE DO
DELETE FROM `mysql_rest_service_metadata`.`audit_log` WHERE changed_at < TIMESTAMP(DATE_SUB(NOW(), INTERVAL 14 DAY))$$


CREATE FUNCTION `mysql_rest_service_metadata`.`valid_request_path`(path VARCHAR(255))
RETURNS TINYINT(1) NOT DETERMINISTIC READS SQL DATA
BEGIN
    SET @valid := (SELECT COUNT(*) = 0 AS valid FROM
        (SELECT CONCAT(COALESCE(se.in_development->>'$.developers', ''), h.name,
            se.url_context_root) as full_request_path
        FROM `mysql_rest_service_metadata`.service se
            LEFT JOIN `mysql_rest_service_metadata`.url_host h
                ON se.url_host_id = h.id
        WHERE CONCAT(COALESCE(se.in_development->>'$.developers', ''), h.name, se.url_context_root) = path
            AND se.enabled = TRUE
        UNION
        SELECT CONCAT(COALESCE(se.in_development->>'$.developers', ''), h.name, se.url_context_root,
            sc.request_path) as full_request_path
        FROM `mysql_rest_service_metadata`.db_schema sc
            LEFT OUTER JOIN `mysql_rest_service_metadata`.service se
                ON se.id = sc.service_id
            LEFT JOIN `mysql_rest_service_metadata`.url_host h
                ON se.url_host_id = h.id
        WHERE CONCAT(COALESCE(se.in_development->>'$.developers', ''), h.name, se.url_context_root,
                sc.request_path) = path
            AND se.enabled = TRUE
        UNION
        SELECT CONCAT(COALESCE(se.in_development->>'$.developers', ''), h.name, se.url_context_root,
            sc.request_path, o.request_path) as full_request_path
        FROM `mysql_rest_service_metadata`.db_object o
            LEFT OUTER JOIN `mysql_rest_service_metadata`.db_schema sc
                ON sc.id = o.db_schema_id
            LEFT OUTER JOIN `mysql_rest_service_metadata`.service se
                ON se.id = sc.service_id
            LEFT JOIN `mysql_rest_service_metadata`.url_host h
                ON se.url_host_id = h.id
        WHERE CONCAT(COALESCE(se.in_development->>'$.developers', ''), h.name, se.url_context_root,
                sc.request_path, o.request_path) = path
            AND se.enabled = TRUE
        UNION
        SELECT CONCAT(COALESCE(se.in_development->>'$.developers', ''), h.name, se.url_context_root,
            co.request_path) as full_request_path
        FROM `mysql_rest_service_metadata`.content_set co
            LEFT OUTER JOIN `mysql_rest_service_metadata`.service se
                ON se.id = co.service_id
            LEFT JOIN `mysql_rest_service_metadata`.url_host h
                ON se.url_host_id = h.id
        WHERE CONCAT(COALESCE(se.in_development->>'$.developers', ''), h.name, se.url_context_root,
                co.request_path) = path
            AND se.enabled = TRUE) AS p);

    RETURN @valid;
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`router_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `router` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "router",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "options", NEW.options),
        NULL,
        UNHEX(LPAD(CONV(NEW.id, 10, 16), 32, '0')),
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`router_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `router` FOR EACH ROW
BEGIN
    IF (COALESCE(OLD.options, '') <> COALESCE(NEW.options, '')) THEN
        INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
            table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
        VALUES (
            "router",
            "UPDATE",
            JSON_OBJECT(
                "id", OLD.id,
                "options", OLD.options),
            JSON_OBJECT(
                "id", NEW.id,
                "options", NEW.options),
            UNHEX(LPAD(CONV(OLD.id, 10, 16), 32, '0')),
            UNHEX(LPAD(CONV(NEW.id, 10, 16), 32, '0')),
            CURRENT_USER(),
            CURRENT_TIMESTAMP
        );
    END IF;
END$$

DELIMITER ;$$

# Ensure that for STORED PROCEDURE parameters at least one of the 'in' and 'out' flag is set to true
ALTER TABLE `mysql_rest_service_metadata`.`object_field`
  ADD CONSTRAINT param_mode_not_false CHECK (
    (db_column->"$.in" IS NULL AND db_column->"$.out" IS NULL) OR
    (db_column->"$.in" + db_column->"$.out" >= 1));

# Ensure the service.in_development->>$.developers is a list that only holds unique strings
ALTER TABLE `mysql_rest_service_metadata`.`service`
  ADD CONSTRAINT in_development_developers_check CHECK(
    JSON_SCHEMA_VALID('{
    "id": "https://dev.mysql.com/mrs/service/in_development",
    "type": "object",
    "properties": {
        "developers": {
            "type": "array",
            "items": {
                "type": "string"
            },
            "minItems": 1,
            "uniqueItems": true
        }
    },
    "required": [ "developers" ]
    }', s.in_development)
);

# -----------------------------------------------------
# Create audit_log triggers
#

DELIMITER $$;
CREATE TRIGGER `mysql_rest_service_metadata`.`db_schema_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `db_schema` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "db_schema",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "service_id", NEW.service_id,
            "name", NEW.name,
            "schema_type", NEW.schema_type,
            "request_path", NEW.request_path,
            "requires_auth", NEW.requires_auth,
            "enabled", NEW.enabled,
            "internal", NEW.internal,
            "items_per_page", NEW.items_per_page,
            "comments", NEW.comments,
            "options", NEW.options,
            "metadata", NEW.metadata),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`db_schema_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `db_schema` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "db_schema",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "service_id", OLD.service_id,
            "name", OLD.name,
            "schema_type", OLD.schema_type,
            "request_path", OLD.request_path,
            "requires_auth", OLD.requires_auth,
            "enabled", OLD.enabled,
            "internal", OLD.internal,
            "items_per_page", OLD.items_per_page,
            "comments", OLD.comments,
            "options", OLD.options,
            "metadata", OLD.metadata),
        JSON_OBJECT(
            "id", NEW.id,
            "service_id", NEW.service_id,
            "name", NEW.name,
            "schema_type", NEW.schema_type,
            "request_path", NEW.request_path,
            "requires_auth", NEW.requires_auth,
            "enabled", NEW.enabled,
            "internal", NEW.internal,
            "items_per_page", NEW.items_per_page,
            "comments", NEW.comments,
            "options", NEW.options,
            "metadata", NEW.metadata),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`db_schema_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `db_schema` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "db_schema",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "service_id", OLD.service_id,
            "name", OLD.name,
            "schema_type", OLD.schema_type,
            "request_path", OLD.request_path,
            "requires_auth", OLD.requires_auth,
            "enabled", OLD.enabled,
            "internal", OLD.internal,
            "items_per_page", OLD.items_per_page,
            "comments", OLD.comments,
            "options", OLD.options,
            "metadata", OLD.metadata),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`service_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `service` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "service",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "parent_id", NEW.parent_id,
            "url_host_id", NEW.url_host_id,
            "url_context_root", NEW.url_context_root,
            "url_protocol", NEW.url_protocol,
            "name", NEW.name,
            "enabled", NEW.enabled,
            "published", NEW.published,
            "in_development", NEW.in_development,
            "comments", NEW.comments,
            "options", NEW.options,
            "auth_path", NEW.auth_path,
            "auth_completed_url", NEW.auth_completed_url,
            "auth_completed_url_validation", NEW.auth_completed_url_validation,
            "enable_sql_endpoint", NEW.enable_sql_endpoint,
            "custom_metadata_schema", NEW.custom_metadata_schema,
            "metadata", NEW.metadata),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`service_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `service` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "service",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "parent_id", OLD.parent_id,
            "url_host_id", OLD.url_host_id,
            "url_context_root", OLD.url_context_root,
            "url_protocol", OLD.url_protocol,
            "name", OLD.name,
            "enabled", OLD.enabled,
            "published", OLD.published,
            "in_development", OLD.in_development,
            "comments", OLD.comments,
            "options", OLD.options,
            "auth_path", OLD.auth_path,
            "auth_completed_url", OLD.auth_completed_url,
            "auth_completed_url_validation", OLD.auth_completed_url_validation,
            "enable_sql_endpoint", OLD.enable_sql_endpoint,
            "custom_metadata_schema", OLD.custom_metadata_schema,
            "metadata", OLD.metadata),
        JSON_OBJECT(
            "id", NEW.id,
            "parent_id", NEW.parent_id,
            "url_host_id", NEW.url_host_id,
            "url_context_root", NEW.url_context_root,
            "url_protocol", NEW.url_protocol,
            "name", NEW.name,
            "enabled", NEW.enabled,
            "published", NEW.published,
            "in_development", NEW.in_development,
            "comments", NEW.comments,
            "options", NEW.options,
            "auth_path", NEW.auth_path,
            "auth_completed_url", NEW.auth_completed_url,
            "auth_completed_url_validation", NEW.auth_completed_url_validation,
            "enable_sql_endpoint", NEW.enable_sql_endpoint,
            "custom_metadata_schema", NEW.custom_metadata_schema,
            "metadata", NEW.metadata),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`service_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `service` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "service",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "parent_id", OLD.parent_id,
            "url_host_id", OLD.url_host_id,
            "url_context_root", OLD.url_context_root,
            "url_protocol", OLD.url_protocol,
            "name", OLD.name,
            "enabled", OLD.enabled,
            "published", OLD.published,
            "in_development", OLD.in_development,
            "comments", OLD.comments,
            "options", OLD.options,
            "auth_path", OLD.auth_path,
            "auth_completed_url", OLD.auth_completed_url,
            "auth_completed_url_validation", OLD.auth_completed_url_validation,
            "enable_sql_endpoint", OLD.enable_sql_endpoint,
            "custom_metadata_schema", OLD.custom_metadata_schema,
            "metadata", OLD.metadata),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`db_object_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `db_object` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "db_object",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "db_schema_id", NEW.db_schema_id,
            "name", NEW.name,
            "request_path", NEW.request_path,
            "enabled", NEW.enabled,
            "internal", NEW.internal,
            "object_type", NEW.object_type,
            "crud_operations", NEW.crud_operations,
            "format", NEW.format,
            "items_per_page", NEW.items_per_page,
            "media_type", NEW.media_type,
            "auto_detect_media_type", NEW.auto_detect_media_type,
            "requires_auth", NEW.requires_auth,
            "auth_stored_procedure", NEW.auth_stored_procedure,
            "options", NEW.options,
            "details", NEW.details,
            "comments", NEW.comments,
            "metadata", NEW.metadata),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`db_object_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `db_object` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "db_object",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "db_schema_id", OLD.db_schema_id,
            "name", OLD.name,
            "request_path", OLD.request_path,
            "enabled", OLD.enabled,
            "internal", OLD.internal,
            "object_type", OLD.object_type,
            "crud_operations", OLD.crud_operations,
            "format", OLD.format,
            "items_per_page", OLD.items_per_page,
            "media_type", OLD.media_type,
            "auto_detect_media_type", OLD.auto_detect_media_type,
            "requires_auth", OLD.requires_auth,
            "auth_stored_procedure", OLD.auth_stored_procedure,
            "options", OLD.options,
            "details", OLD.details,
            "comments", OLD.comments,
            "metadata", OLD.metadata),
        JSON_OBJECT(
            "id", NEW.id,
            "db_schema_id", NEW.db_schema_id,
            "name", NEW.name,
            "request_path", NEW.request_path,
            "enabled", NEW.enabled,
            "internal", NEW.internal,
            "object_type", NEW.object_type,
            "crud_operations", NEW.crud_operations,
            "format", NEW.format,
            "items_per_page", NEW.items_per_page,
            "media_type", NEW.media_type,
            "auto_detect_media_type", NEW.auto_detect_media_type,
            "requires_auth", NEW.requires_auth,
            "auth_stored_procedure", NEW.auth_stored_procedure,
            "options", NEW.options,
            "details", NEW.details,
            "comments", NEW.comments,
            "metadata", NEW.metadata),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`db_object_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `db_object` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "db_object",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "db_schema_id", OLD.db_schema_id,
            "name", OLD.name,
            "request_path", OLD.request_path,
            "enabled", OLD.enabled,
            "internal", OLD.internal,
            "object_type", OLD.object_type,
            "crud_operations", OLD.crud_operations,
            "format", OLD.format,
            "items_per_page", OLD.items_per_page,
            "media_type", OLD.media_type,
            "auto_detect_media_type", OLD.auto_detect_media_type,
            "requires_auth", OLD.requires_auth,
            "auth_stored_procedure", OLD.auth_stored_procedure,
            "options", OLD.options,
            "details", OLD.details,
            "comments", OLD.comments,
            "metadata", OLD.metadata),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `mrs_user` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "auth_app_id", NEW.auth_app_id,
            "name", NEW.name,
            "email", NEW.email,
            "vendor_user_id", NEW.vendor_user_id,
            "login_permitted", NEW.login_permitted,
            "mapped_user_id", NEW.mapped_user_id,
            "app_options", NEW.app_options,
            "options", NEW.options),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `mrs_user` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "auth_app_id", OLD.auth_app_id,
            "name", OLD.name,
            "email", OLD.email,
            "vendor_user_id", OLD.vendor_user_id,
            "login_permitted", OLD.login_permitted,
            "mapped_user_id", OLD.mapped_user_id,
            "app_options", OLD.app_options,
            "options", OLD.options),
        JSON_OBJECT(
            "id", NEW.id,
            "auth_app_id", NEW.auth_app_id,
            "name", NEW.name,
            "email", NEW.email,
            "vendor_user_id", NEW.vendor_user_id,
            "login_permitted", NEW.login_permitted,
            "mapped_user_id", NEW.mapped_user_id,
            "app_options", NEW.app_options,
            "options", NEW.options),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `mrs_user` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "auth_app_id", OLD.auth_app_id,
            "name", OLD.name,
            "email", OLD.email,
            "vendor_user_id", OLD.vendor_user_id,
            "login_permitted", OLD.login_permitted,
            "mapped_user_id", OLD.mapped_user_id,
            "app_options", OLD.app_options,
            "options", OLD.options),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`auth_vendor_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `auth_vendor` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "auth_vendor",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "name", NEW.name,
            "validation_url", NEW.validation_url,
            "enabled", NEW.enabled,
            "comments", NEW.comments,
            "options", NEW.options),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`auth_vendor_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `auth_vendor` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "auth_vendor",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "name", OLD.name,
            "validation_url", OLD.validation_url,
            "enabled", OLD.enabled,
            "comments", OLD.comments,
            "options", OLD.options),
        JSON_OBJECT(
            "id", NEW.id,
            "name", NEW.name,
            "validation_url", NEW.validation_url,
            "enabled", NEW.enabled,
            "comments", NEW.comments,
            "options", NEW.options),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`auth_vendor_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `auth_vendor` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "auth_vendor",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "name", OLD.name,
            "validation_url", OLD.validation_url,
            "enabled", OLD.enabled,
            "comments", OLD.comments,
            "options", OLD.options),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`auth_app_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `auth_app` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "auth_app",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "auth_vendor_id", NEW.auth_vendor_id,
            "name", NEW.name,
            "description", NEW.description,
            "url", NEW.url,
            "url_direct_auth", NEW.url_direct_auth,
            "access_token", NEW.access_token,
            "app_id", NEW.app_id,
            "enabled", NEW.enabled,
            "limit_to_registered_users", NEW.limit_to_registered_users,
            "default_role_id", NEW.default_role_id,
            "options", NEW.options),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`auth_app_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `auth_app` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "auth_app",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "auth_vendor_id", OLD.auth_vendor_id,
            "name", OLD.name,
            "description", OLD.description,
            "url", OLD.url,
            "url_direct_auth", OLD.url_direct_auth,
            "access_token", OLD.access_token,
            "app_id", OLD.app_id,
            "enabled", OLD.enabled,
            "limit_to_registered_users", OLD.limit_to_registered_users,
            "default_role_id", OLD.default_role_id,
            "options", OLD.options),
        JSON_OBJECT(
            "id", NEW.id,
            "auth_vendor_id", NEW.auth_vendor_id,
            "name", NEW.name,
            "description", NEW.description,
            "url", NEW.url,
            "url_direct_auth", NEW.url_direct_auth,
            "access_token", NEW.access_token,
            "app_id", NEW.app_id,
            "enabled", NEW.enabled,
            "limit_to_registered_users", NEW.limit_to_registered_users,
            "default_role_id", NEW.default_role_id,
            "options", NEW.options),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`auth_app_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `auth_app` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "auth_app",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "auth_vendor_id", OLD.auth_vendor_id,
            "name", OLD.name,
            "description", OLD.description,
            "url", OLD.url,
            "url_direct_auth", OLD.url_direct_auth,
            "access_token", OLD.access_token,
            "app_id", OLD.app_id,
            "enabled", OLD.enabled,
            "limit_to_registered_users", OLD.limit_to_registered_users,
            "default_role_id", OLD.default_role_id,
            "options", OLD.options),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`config_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `config` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "config",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "service_enabled", NEW.service_enabled,
            "data", NEW.data),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`config_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `config` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "config",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "service_enabled", OLD.service_enabled,
            "data", OLD.data),
        JSON_OBJECT(
            "id", NEW.id,
            "service_enabled", NEW.service_enabled,
            "data", NEW.data),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`config_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `config` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "config",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "service_enabled", OLD.service_enabled,
            "data", OLD.data),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`redirect_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `redirect` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "redirect",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "pattern", NEW.pattern,
            "target", NEW.target),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`redirect_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `redirect` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "redirect",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "pattern", OLD.pattern,
            "target", OLD.target),
        JSON_OBJECT(
            "id", NEW.id,
            "pattern", NEW.pattern,
            "target", NEW.target),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`redirect_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `redirect` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "redirect",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "pattern", OLD.pattern,
            "target", OLD.target),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`url_host_alias_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `url_host_alias` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "url_host_alias",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "url_host_id", NEW.url_host_id,
            "alias", NEW.alias),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`url_host_alias_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `url_host_alias` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "url_host_alias",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "url_host_id", OLD.url_host_id,
            "alias", OLD.alias),
        JSON_OBJECT(
            "id", NEW.id,
            "url_host_id", NEW.url_host_id,
            "alias", NEW.alias),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`url_host_alias_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `url_host_alias` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "url_host_alias",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "url_host_id", OLD.url_host_id,
            "alias", OLD.alias),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`url_host_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `url_host` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "url_host",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "name", NEW.name,
            "comments", NEW.comments),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`url_host_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `url_host` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "url_host",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "name", OLD.name,
            "comments", OLD.comments),
        JSON_OBJECT(
            "id", NEW.id,
            "name", NEW.name,
            "comments", NEW.comments),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`url_host_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `url_host` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "url_host",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "name", OLD.name,
            "comments", OLD.comments),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`content_file_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `content_file` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "content_file",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "content_set_id", NEW.content_set_id,
            "request_path", NEW.request_path,
            "requires_auth", NEW.requires_auth,
            "enabled", NEW.enabled,
            "size", NEW.size,
            "options", NEW.options),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`content_file_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `content_file` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "content_file",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "content_set_id", OLD.content_set_id,
            "request_path", OLD.request_path,
            "requires_auth", OLD.requires_auth,
            "enabled", OLD.enabled,
            "size", OLD.size,
            "options", OLD.options),
        JSON_OBJECT(
            "id", NEW.id,
            "content_set_id", NEW.content_set_id,
            "request_path", NEW.request_path,
            "requires_auth", NEW.requires_auth,
            "enabled", NEW.enabled,
            "size", NEW.size,
            "options", NEW.options),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`content_file_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `content_file` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "content_file",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "content_set_id", OLD.content_set_id,
            "request_path", OLD.request_path,
            "requires_auth", OLD.requires_auth,
            "enabled", OLD.enabled,
            "size", OLD.size,
            "options", OLD.options),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`content_set_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `content_set` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "content_set",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "service_id", NEW.service_id,
            "content_type", NEW.content_type,
            "request_path", NEW.request_path,
            "requires_auth", NEW.requires_auth,
            "enabled", NEW.enabled,
            "internal", NEW.internal,
            "comments", NEW.comments,
            "options", NEW.options),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`content_set_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `content_set` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "content_set",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "service_id", OLD.service_id,
            "content_type", OLD.content_type,
            "request_path", OLD.request_path,
            "requires_auth", OLD.requires_auth,
            "enabled", OLD.enabled,
            "internal", OLD.internal,
            "comments", OLD.comments,
            "options", OLD.options),
        JSON_OBJECT(
            "id", NEW.id,
            "service_id", NEW.service_id,
            "content_type", NEW.content_type,
            "request_path", NEW.request_path,
            "requires_auth", NEW.requires_auth,
            "enabled", NEW.enabled,
            "internal", NEW.internal,
            "comments", NEW.comments,
            "options", NEW.options),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`content_set_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `content_set` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "content_set",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "service_id", OLD.service_id,
            "content_type", OLD.content_type,
            "request_path", OLD.request_path,
            "requires_auth", OLD.requires_auth,
            "enabled", OLD.enabled,
            "internal", OLD.internal,
            "comments", OLD.comments,
            "options", OLD.options),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_role_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `mrs_role` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_role",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "derived_from_role_id", NEW.derived_from_role_id,
            "specific_to_service_id", NEW.specific_to_service_id,
            "caption", NEW.caption,
            "description", NEW.description,
            "options", NEW.options),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_role_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `mrs_role` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_role",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "derived_from_role_id", OLD.derived_from_role_id,
            "specific_to_service_id", OLD.specific_to_service_id,
            "caption", OLD.caption,
            "description", OLD.description,
            "options", OLD.options),
        JSON_OBJECT(
            "id", NEW.id,
            "derived_from_role_id", NEW.derived_from_role_id,
            "specific_to_service_id", NEW.specific_to_service_id,
            "caption", NEW.caption,
            "description", NEW.description,
            "options", NEW.options),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_role_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `mrs_role` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_role",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "derived_from_role_id", OLD.derived_from_role_id,
            "specific_to_service_id", OLD.specific_to_service_id,
            "caption", OLD.caption,
            "description", OLD.description,
            "options", OLD.options),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_has_role_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `mrs_user_has_role` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_has_role",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "user_id", NEW.user_id,
            "role_id", NEW.role_id,
            "comments", NEW.comments,
            "options", NEW.options),
        NULL,
        NEW.user_id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_has_role_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `mrs_user_has_role` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_has_role",
        "UPDATE",
        JSON_OBJECT(
            "user_id", OLD.user_id,
            "role_id", OLD.role_id,
            "comments", OLD.comments,
            "options", OLD.options),
        JSON_OBJECT(
            "user_id", NEW.user_id,
            "role_id", NEW.role_id,
            "comments", NEW.comments,
            "options", NEW.options),
        OLD.user_id,
        NEW.user_id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_has_role_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `mrs_user_has_role` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_has_role",
        "DELETE",
        JSON_OBJECT(
            "user_id", OLD.user_id,
            "role_id", OLD.role_id,
            "comments", OLD.comments,
            "options", OLD.options),
        NULL,
        OLD.user_id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_hierarchy_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `mrs_user_hierarchy` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_hierarchy",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "user_id", NEW.user_id,
            "reporting_to_user_id", NEW.reporting_to_user_id,
            "user_hierarchy_type_id", NEW.user_hierarchy_type_id,
            "options", NEW.options),
        NULL,
        NEW.user_id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_hierarchy_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `mrs_user_hierarchy` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_hierarchy",
        "UPDATE",
        JSON_OBJECT(
            "user_id", OLD.user_id,
            "reporting_to_user_id", OLD.reporting_to_user_id,
            "user_hierarchy_type_id", OLD.user_hierarchy_type_id,
            "options", OLD.options),
        JSON_OBJECT(
            "user_id", NEW.user_id,
            "reporting_to_user_id", NEW.reporting_to_user_id,
            "user_hierarchy_type_id", NEW.user_hierarchy_type_id,
            "options", NEW.options),
        OLD.user_id,
        NEW.user_id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_hierarchy_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `mrs_user_hierarchy` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_hierarchy",
        "DELETE",
        JSON_OBJECT(
            "user_id", OLD.user_id,
            "reporting_to_user_id", OLD.reporting_to_user_id,
            "user_hierarchy_type_id", OLD.user_hierarchy_type_id,
            "options", OLD.options),
        NULL,
        OLD.user_id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_hierarchy_type_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `mrs_user_hierarchy_type` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_hierarchy_type",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "caption", NEW.caption,
            "description", NEW.description,
            "specific_to_service_id", NEW.specific_to_service_id,
            "options", NEW.options),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_hierarchy_type_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `mrs_user_hierarchy_type` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_hierarchy_type",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "caption", OLD.caption,
            "description", OLD.description,
            "specific_to_service_id", OLD.specific_to_service_id,
            "options", OLD.options),
        JSON_OBJECT(
            "id", NEW.id,
            "caption", NEW.caption,
            "description", NEW.description,
            "specific_to_service_id", NEW.specific_to_service_id,
            "options", NEW.options),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_hierarchy_type_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `mrs_user_hierarchy_type` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_hierarchy_type",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "caption", OLD.caption,
            "description", OLD.description,
            "specific_to_service_id", OLD.specific_to_service_id,
            "options", OLD.options),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_privilege_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `mrs_privilege` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_privilege",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "role_id", NEW.role_id,
            "crud_operations", NEW.crud_operations,
            "service_id", NEW.service_id,
            "db_schema_id", NEW.db_schema_id,
            "db_object_id", NEW.db_object_id,
            "service_path", NEW.service_path,
            "schema_path", NEW.schema_path,
            "object_path", NEW.object_path,
            "options", NEW.options),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_privilege_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `mrs_privilege` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_privilege",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "role_id", OLD.role_id,
            "crud_operations", OLD.crud_operations,
            "service_id", OLD.service_id,
            "db_schema_id", OLD.db_schema_id,
            "db_object_id", OLD.db_object_id,
            "service_path", OLD.service_path,
            "schema_path", OLD.schema_path,
            "object_path", OLD.object_path,
            "options", OLD.options),
        JSON_OBJECT(
            "id", NEW.id,
            "role_id", NEW.role_id,
            "crud_operations", NEW.crud_operations,
            "service_id", NEW.service_id,
            "db_schema_id", NEW.db_schema_id,
            "db_object_id", NEW.db_object_id,
            "service_path", NEW.service_path,
            "schema_path", NEW.schema_path,
            "object_path", NEW.object_path,
            "options", NEW.options),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_privilege_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `mrs_privilege` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_privilege",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "role_id", OLD.role_id,
            "crud_operations", OLD.crud_operations,
            "service_id", OLD.service_id,
            "db_schema_id", OLD.db_schema_id,
            "db_object_id", OLD.db_object_id,
            "service_path", OLD.service_path,
            "schema_path", OLD.schema_path,
            "object_path", OLD.object_path,
            "options", OLD.options),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_group_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `mrs_user_group` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_group",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "specific_to_service_id", NEW.specific_to_service_id,
            "caption", NEW.caption,
            "description", NEW.description,
            "options", NEW.options),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_group_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `mrs_user_group` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_group",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "specific_to_service_id", OLD.specific_to_service_id,
            "caption", OLD.caption,
            "description", OLD.description,
            "options", OLD.options),
        JSON_OBJECT(
            "id", NEW.id,
            "specific_to_service_id", NEW.specific_to_service_id,
            "caption", NEW.caption,
            "description", NEW.description,
            "options", NEW.options),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_group_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `mrs_user_group` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_group",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "specific_to_service_id", OLD.specific_to_service_id,
            "caption", OLD.caption,
            "description", OLD.description,
            "options", OLD.options),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_group_has_role_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `mrs_user_group_has_role` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_group_has_role",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "user_group_id", NEW.user_group_id,
            "role_id", NEW.role_id,
            "options", NEW.options),
        NULL,
        NEW.user_group_id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_group_has_role_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `mrs_user_group_has_role` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_group_has_role",
        "UPDATE",
        JSON_OBJECT(
            "user_group_id", OLD.user_group_id,
            "role_id", OLD.role_id,
            "options", OLD.options),
        JSON_OBJECT(
            "user_group_id", NEW.user_group_id,
            "role_id", NEW.role_id,
            "options", NEW.options),
        OLD.user_group_id,
        NEW.user_group_id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_group_has_role_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `mrs_user_group_has_role` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_group_has_role",
        "DELETE",
        JSON_OBJECT(
            "user_group_id", OLD.user_group_id,
            "role_id", OLD.role_id,
            "options", OLD.options),
        NULL,
        OLD.user_group_id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_has_group_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `mrs_user_has_group` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_has_group",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "user_id", NEW.user_id,
            "user_group_id", NEW.user_group_id,
            "comments", NEW.comments,
            "options", NEW.options),
        NULL,
        NEW.user_id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_has_group_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `mrs_user_has_group` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_has_group",
        "UPDATE",
        JSON_OBJECT(
            "user_id", OLD.user_id,
            "user_group_id", OLD.user_group_id,
            "comments", OLD.comments,
            "options", OLD.options),
        JSON_OBJECT(
            "user_id", NEW.user_id,
            "user_group_id", NEW.user_group_id,
            "comments", NEW.comments,
            "options", NEW.options),
        OLD.user_id,
        NEW.user_id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_has_group_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `mrs_user_has_group` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_has_group",
        "DELETE",
        JSON_OBJECT(
            "user_id", OLD.user_id,
            "user_group_id", OLD.user_group_id,
            "comments", OLD.comments,
            "options", OLD.options),
        NULL,
        OLD.user_id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_group_hierarchy_type_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `mrs_group_hierarchy_type` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_group_hierarchy_type",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "caption", NEW.caption,
            "description", NEW.description,
            "options", NEW.options),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_group_hierarchy_type_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `mrs_group_hierarchy_type` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_group_hierarchy_type",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "caption", OLD.caption,
            "description", OLD.description,
            "options", OLD.options),
        JSON_OBJECT(
            "id", NEW.id,
            "caption", NEW.caption,
            "description", NEW.description,
            "options", NEW.options),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_group_hierarchy_type_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `mrs_group_hierarchy_type` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_group_hierarchy_type",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "caption", OLD.caption,
            "description", OLD.description,
            "options", OLD.options),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_group_hierarchy_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `mrs_user_group_hierarchy` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_group_hierarchy",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "user_group_id", NEW.user_group_id,
            "parent_group_id", NEW.parent_group_id,
            "group_hierarchy_type_id", NEW.group_hierarchy_type_id,
            "level", NEW.level,
            "options", NEW.options),
        NULL,
        NEW.user_group_id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_group_hierarchy_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `mrs_user_group_hierarchy` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_group_hierarchy",
        "UPDATE",
        JSON_OBJECT(
            "user_group_id", OLD.user_group_id,
            "parent_group_id", OLD.parent_group_id,
            "group_hierarchy_type_id", OLD.group_hierarchy_type_id,
            "level", OLD.level,
            "options", OLD.options),
        JSON_OBJECT(
            "user_group_id", NEW.user_group_id,
            "parent_group_id", NEW.parent_group_id,
            "group_hierarchy_type_id", NEW.group_hierarchy_type_id,
            "level", NEW.level,
            "options", NEW.options),
        OLD.user_group_id,
        NEW.user_group_id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_user_group_hierarchy_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `mrs_user_group_hierarchy` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_user_group_hierarchy",
        "DELETE",
        JSON_OBJECT(
            "user_group_id", OLD.user_group_id,
            "parent_group_id", OLD.parent_group_id,
            "group_hierarchy_type_id", OLD.group_hierarchy_type_id,
            "level", OLD.level,
            "options", OLD.options),
        NULL,
        OLD.user_group_id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_db_object_row_group_security_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `mrs_db_object_row_group_security` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_db_object_row_group_security",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "db_object_id", NEW.db_object_id,
            "group_hierarchy_type_id", NEW.group_hierarchy_type_id,
            "row_group_ownership_column", NEW.row_group_ownership_column,
            "level", NEW.level,
            "match_level", NEW.match_level,
            "options", NEW.options),
        NULL,
        NEW.db_object_id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_db_object_row_group_security_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `mrs_db_object_row_group_security` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_db_object_row_group_security",
        "UPDATE",
        JSON_OBJECT(
            "db_object_id", OLD.db_object_id,
            "group_hierarchy_type_id", OLD.group_hierarchy_type_id,
            "row_group_ownership_column", OLD.row_group_ownership_column,
            "level", OLD.level,
            "match_level", OLD.match_level,
            "options", OLD.options),
        JSON_OBJECT(
            "db_object_id", NEW.db_object_id,
            "group_hierarchy_type_id", NEW.group_hierarchy_type_id,
            "row_group_ownership_column", NEW.row_group_ownership_column,
            "level", NEW.level,
            "match_level", NEW.match_level,
            "options", NEW.options),
        OLD.db_object_id,
        NEW.db_object_id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`mrs_db_object_row_group_security_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `mrs_db_object_row_group_security` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "mrs_db_object_row_group_security",
        "DELETE",
        JSON_OBJECT(
            "db_object_id", OLD.db_object_id,
            "group_hierarchy_type_id", OLD.group_hierarchy_type_id,
            "row_group_ownership_column", OLD.row_group_ownership_column,
            "level", OLD.level,
            "match_level", OLD.match_level,
            "options", OLD.options),
        NULL,
        OLD.db_object_id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`object_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `object` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "object",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "db_object_id", NEW.db_object_id,
            "name", NEW.name,
            "kind", NEW.kind,
            "position", NEW.position,
            "row_ownership_field_id", NEW.row_ownership_field_id,
            "options", NEW.options,
            "sdk_options", NEW.sdk_options,
            "comments", NEW.comments),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`object_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `object` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "object",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "db_object_id", OLD.db_object_id,
            "name", OLD.name,
            "kind", OLD.kind,
            "position", OLD.position,
            "row_ownership_field_id", OLD.row_ownership_field_id,
            "options", OLD.options,
            "sdk_options", OLD.sdk_options,
            "comments", OLD.comments),
        JSON_OBJECT(
            "id", NEW.id,
            "db_object_id", NEW.db_object_id,
            "name", NEW.name,
            "kind", NEW.kind,
            "position", NEW.position,
            "row_ownership_field_id", NEW.row_ownership_field_id,
            "options", NEW.options,
            "sdk_options", NEW.sdk_options,
            "comments", NEW.comments),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`object_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `object` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "object",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "db_object_id", OLD.db_object_id,
            "name", OLD.name,
            "kind", OLD.kind,
            "position", OLD.position,
            "row_ownership_field_id", OLD.row_ownership_field_id,
            "options", OLD.options,
            "sdk_options", OLD.sdk_options,
            "comments", OLD.comments),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`object_field_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `object_field` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "object_field",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "object_id", NEW.object_id,
            "parent_reference_id", NEW.parent_reference_id,
            "represents_reference_id", NEW.represents_reference_id,
            "name", NEW.name,
            "position", NEW.position,
            "db_column", NEW.db_column,
            "enabled", NEW.enabled,
            "allow_filtering", NEW.allow_filtering,
            "allow_sorting", NEW.allow_sorting,
            "no_check", NEW.no_check,
            "no_update", NEW.no_update,
            "options", NEW.options,
            "sdk_options", NEW.sdk_options,
            "comments", NEW.comments),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`object_field_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `object_field` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "object_field",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "object_id", OLD.object_id,
            "parent_reference_id", OLD.parent_reference_id,
            "represents_reference_id", OLD.represents_reference_id,
            "name", OLD.name,
            "position", OLD.position,
            "db_column", OLD.db_column,
            "enabled", OLD.enabled,
            "allow_filtering", OLD.allow_filtering,
            "allow_sorting", OLD.allow_sorting,
            "no_check", OLD.no_check,
            "no_update", OLD.no_update,
            "options", OLD.options,
            "sdk_options", OLD.sdk_options,
            "comments", OLD.comments),
        JSON_OBJECT(
            "id", NEW.id,
            "object_id", NEW.object_id,
            "parent_reference_id", NEW.parent_reference_id,
            "represents_reference_id", NEW.represents_reference_id,
            "name", NEW.name,
            "position", NEW.position,
            "db_column", NEW.db_column,
            "enabled", NEW.enabled,
            "allow_filtering", NEW.allow_filtering,
            "allow_sorting", NEW.allow_sorting,
            "no_check", NEW.no_check,
            "no_update", NEW.no_update,
            "options", NEW.options,
            "sdk_options", NEW.sdk_options,
            "comments", NEW.comments),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`object_field_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `object_field` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "object_field",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "object_id", OLD.object_id,
            "parent_reference_id", OLD.parent_reference_id,
            "represents_reference_id", OLD.represents_reference_id,
            "name", OLD.name,
            "position", OLD.position,
            "db_column", OLD.db_column,
            "enabled", OLD.enabled,
            "allow_filtering", OLD.allow_filtering,
            "allow_sorting", OLD.allow_sorting,
            "no_check", OLD.no_check,
            "no_update", OLD.no_update,
            "options", OLD.options,
            "sdk_options", OLD.sdk_options,
            "comments", OLD.comments),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`object_reference_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `object_reference` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "object_reference",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "id", NEW.id,
            "reduce_to_value_of_field_id", NEW.reduce_to_value_of_field_id,
            "row_ownership_field_id", NEW.row_ownership_field_id,
            "reference_mapping", NEW.reference_mapping,
            "unnest", NEW.unnest,
            "options", NEW.options,
            "sdk_options", NEW.sdk_options,
            "comments", NEW.comments),
        NULL,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`object_reference_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `object_reference` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "object_reference",
        "UPDATE",
        JSON_OBJECT(
            "id", OLD.id,
            "reduce_to_value_of_field_id", OLD.reduce_to_value_of_field_id,
            "row_ownership_field_id", OLD.row_ownership_field_id,
            "reference_mapping", OLD.reference_mapping,
            "unnest", OLD.unnest,
            "options", OLD.options,
            "sdk_options", OLD.sdk_options,
            "comments", OLD.comments),
        JSON_OBJECT(
            "id", NEW.id,
            "reduce_to_value_of_field_id", NEW.reduce_to_value_of_field_id,
            "row_ownership_field_id", NEW.row_ownership_field_id,
            "reference_mapping", NEW.reference_mapping,
            "unnest", NEW.unnest,
            "options", NEW.options,
            "sdk_options", NEW.sdk_options,
            "comments", NEW.comments),
        OLD.id,
        NEW.id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`object_reference_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `object_reference` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "object_reference",
        "DELETE",
        JSON_OBJECT(
            "id", OLD.id,
            "reduce_to_value_of_field_id", OLD.reduce_to_value_of_field_id,
            "row_ownership_field_id", OLD.row_ownership_field_id,
            "reference_mapping", OLD.reference_mapping,
            "unnest", OLD.unnest,
            "options", OLD.options,
            "sdk_options", OLD.sdk_options,
            "comments", OLD.comments),
        NULL,
        OLD.id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`service_has_auth_app_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `service_has_auth_app` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "service_has_auth_app",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "service_id", NEW.service_id,
            "auth_app_id", NEW.auth_app_id,
            "options", NEW.options),
        NULL,
        NEW.service_id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`service_has_auth_app_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `service_has_auth_app` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "service_has_auth_app",
        "UPDATE",
        JSON_OBJECT(
            "service_id", OLD.service_id,
            "auth_app_id", OLD.auth_app_id,
            "options", OLD.options),
        JSON_OBJECT(
            "service_id", NEW.service_id,
            "auth_app_id", NEW.auth_app_id,
            "options", NEW.options),
        OLD.service_id,
        NEW.service_id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`service_has_auth_app_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `service_has_auth_app` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "service_has_auth_app",
        "DELETE",
        JSON_OBJECT(
            "service_id", OLD.service_id,
            "auth_app_id", OLD.auth_app_id,
            "options", OLD.options),
        NULL,
        OLD.service_id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`content_set_has_obj_def_AFTER_INSERT_AUDIT_LOG` AFTER INSERT ON `content_set_has_obj_def` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "content_set_has_obj_def",
        "INSERT",
        NULL,
        JSON_OBJECT(
            "content_set_id", NEW.content_set_id,
            "db_object_id", NEW.db_object_id,
            "kind", NEW.kind,
            "priority", NEW.priority,
            "language", NEW.language,
            "name", NEW.name,
            "class_name", NEW.class_name,
            "comments", NEW.comments,
            "options", NEW.options),
        NULL,
        NEW.content_set_id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`content_set_has_obj_def_AFTER_UPDATE_AUDIT_LOG` AFTER UPDATE ON `content_set_has_obj_def` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "content_set_has_obj_def",
        "UPDATE",
        JSON_OBJECT(
            "content_set_id", OLD.content_set_id,
            "db_object_id", OLD.db_object_id,
            "kind", OLD.kind,
            "priority", OLD.priority,
            "language", OLD.language,
            "name", OLD.name,
            "class_name", OLD.class_name,
            "comments", OLD.comments,
            "options", OLD.options),
        JSON_OBJECT(
            "content_set_id", NEW.content_set_id,
            "db_object_id", NEW.db_object_id,
            "kind", NEW.kind,
            "priority", NEW.priority,
            "language", NEW.language,
            "name", NEW.name,
            "class_name", NEW.class_name,
            "comments", NEW.comments,
            "options", NEW.options),
        OLD.content_set_id,
        NEW.content_set_id,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

CREATE TRIGGER `mysql_rest_service_metadata`.`content_set_has_obj_def_AFTER_DELETE_AUDIT_LOG` AFTER DELETE ON `content_set_has_obj_def` FOR EACH ROW
BEGIN
    INSERT INTO `mysql_rest_service_metadata`.`audit_log` (
        table_name, dml_type, old_row_data, new_row_data, old_row_id, new_row_id, changed_by, changed_at)
    VALUES (
        "content_set_has_obj_def",
        "DELETE",
        JSON_OBJECT(
            "content_set_id", OLD.content_set_id,
            "db_object_id", OLD.db_object_id,
            "kind", OLD.kind,
            "priority", OLD.priority,
            "language", OLD.language,
            "name", OLD.name,
            "class_name", OLD.class_name,
            "comments", OLD.comments,
            "options", OLD.options),
        NULL,
        OLD.content_set_id,
        NULL,
        CURRENT_USER(),
        CURRENT_TIMESTAMP
    );
END$$

DELIMITER ;$$

# -----------------------------------------------------
# Create roles for the MySQL REST Service

# The mysql_rest_service_admin ROLE allows to fully manage the REST services
# The mysql_rest_service_schema_admin ROLE allows to manage the database schemas assigned to REST services
# The mysql_rest_service_dev ROLE allows to develop new REST objects for given REST services and upload static files
# The mysql_rest_service_meta_provider ROLE is used by the MySQL Router to read the mrs metadata and make inserts into the auth_user table
# The mysql_rest_service_data_provider ROLE is used by the MySQL Router to read the actual schema data that is exposed via REST

CREATE ROLE IF NOT EXISTS 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev', 'mysql_rest_service_meta_provider', 'mysql_rest_service_data_provider';

# Allow the 'mysql_rest_service_data_provider' role to create temporary tables
GRANT CREATE TEMPORARY TABLES ON *.*
    TO 'mysql_rest_service_data_provider';

# `mysql_rest_service_metadata`.`schema_version`
GRANT SELECT ON `mysql_rest_service_metadata`.`schema_version`
    TO 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev', 'mysql_rest_service_meta_provider';

# `mysql_rest_service_metadata`.`audit_log`
GRANT SELECT ON `mysql_rest_service_metadata`.`audit_log`
    TO 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev', 'mysql_rest_service_meta_provider';

# -----------------------------------------------------
# Config

# `mysql_rest_service_metadata`.`config`
GRANT SELECT, UPDATE
    ON `mysql_rest_service_metadata`.`config`
    TO 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin';
GRANT SELECT ON `mysql_rest_service_metadata`.`config`
    TO 'mysql_rest_service_meta_provider', 'mysql_rest_service_dev';

# `mysql_rest_service_metadata`.`redirect`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`redirect`
    TO 'mysql_rest_service_admin';
GRANT SELECT ON `mysql_rest_service_metadata`.`redirect`
    TO 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev', 'mysql_rest_service_meta_provider';

# -----------------------------------------------------
# Service

# `mysql_rest_service_metadata`.`url_host`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`url_host`
    TO 'mysql_rest_service_admin';
GRANT SELECT ON `mysql_rest_service_metadata`.`url_host`
    TO 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev', 'mysql_rest_service_meta_provider';

# `mysql_rest_service_metadata`.`url_host_alias`
GRANT SELECT, INSERT, DELETE
    ON `mysql_rest_service_metadata`.`url_host_alias`
    TO 'mysql_rest_service_admin';
GRANT SELECT ON `mysql_rest_service_metadata`.`url_host_alias`
    TO 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev', 'mysql_rest_service_meta_provider';

# `mysql_rest_service_metadata`.`service`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`service`
    TO 'mysql_rest_service_admin';
GRANT SELECT ON `mysql_rest_service_metadata`.`service`
    TO 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev', 'mysql_rest_service_meta_provider';

# -----------------------------------------------------
# Schema Objects

# `mysql_rest_service_metadata`.`db_schema`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`db_schema`
    TO 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin';
GRANT SELECT ON `mysql_rest_service_metadata`.`db_schema`
    TO 'mysql_rest_service_meta_provider', 'mysql_rest_service_dev';

# `mysql_rest_service_metadata`.`db_object`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`db_object`
    TO 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev';
GRANT SELECT ON `mysql_rest_service_metadata`.`db_object`
    TO 'mysql_rest_service_meta_provider';

# `mysql_rest_service_metadata`.`mrs_db_object_row_group_security`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`mrs_db_object_row_group_security`
    TO 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev';
GRANT SELECT ON `mysql_rest_service_metadata`.`mrs_db_object_row_group_security`
    TO 'mysql_rest_service_meta_provider';

# `mysql_rest_service_metadata`.`object`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`object`
    TO 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev';
GRANT SELECT ON `mysql_rest_service_metadata`.`object`
    TO 'mysql_rest_service_meta_provider';

# `mysql_rest_service_metadata`.`object_field`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`object_field`
    TO 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev';
GRANT SELECT ON `mysql_rest_service_metadata`.`object_field`
    TO 'mysql_rest_service_meta_provider';

# `mysql_rest_service_metadata`.`object_reference`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`object_reference`
    TO 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev';
GRANT SELECT ON `mysql_rest_service_metadata`.`object_reference`
    TO 'mysql_rest_service_meta_provider';

# `mysql_rest_service_metadata`.`table_columns_with_references`
GRANT SELECT
    ON `mysql_rest_service_metadata`.`table_columns_with_references`
    TO 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev', 'mysql_rest_service_meta_provider';

# `mysql_rest_service_metadata`.`object_fields_with_references`
GRANT SELECT
    ON `mysql_rest_service_metadata`.`object_fields_with_references`
    TO 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev', 'mysql_rest_service_meta_provider';

# -----------------------------------------------------
# Static Content

# `mysql_rest_service_metadata`.`content_set`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`content_set`
    TO 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev';
GRANT SELECT ON `mysql_rest_service_metadata`.`content_set`
    TO 'mysql_rest_service_meta_provider';

# `mysql_rest_service_metadata`.`content_file`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`content_file`
    TO 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev';
GRANT SELECT ON `mysql_rest_service_metadata`.`content_file`
    TO 'mysql_rest_service_meta_provider';


# `mysql_rest_service_metadata`.`content_set_has_obj_def`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`content_set_has_obj_def`
    TO 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev';
GRANT SELECT ON `mysql_rest_service_metadata`.`content_set_has_obj_def`
    TO 'mysql_rest_service_meta_provider';

# -----------------------------------------------------
# User Authentication

# `mysql_rest_service_metadata`.`auth_app`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`auth_app`
    TO 'mysql_rest_service_admin';
GRANT SELECT ON `mysql_rest_service_metadata`.`auth_app`
    TO 'mysql_rest_service_meta_provider', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev';

# `mysql_rest_service_metadata`.`service_has_auth_app`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`service_has_auth_app`
    TO 'mysql_rest_service_admin';
GRANT SELECT ON `mysql_rest_service_metadata`.`service_has_auth_app`
    TO 'mysql_rest_service_meta_provider', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev';

# `mysql_rest_service_metadata`.`auth_vendor`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`auth_vendor`
    TO 'mysql_rest_service_admin';
GRANT SELECT ON `mysql_rest_service_metadata`.`auth_vendor`
    TO 'mysql_rest_service_meta_provider', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev';

# `mysql_rest_service_metadata`.`mrs_user`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`mrs_user`
    TO 'mysql_rest_service_admin';
GRANT SELECT, INSERT ON `mysql_rest_service_metadata`.`mrs_user`
    TO 'mysql_rest_service_meta_provider';
GRANT SELECT ON `mysql_rest_service_metadata`.`mrs_user`
    TO 'mysql_rest_service_data_provider';

# -----------------------------------------------------
# User Hierarchy

# `mysql_rest_service_metadata`.`mrs_user_hierarchy`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`mrs_user_hierarchy`
    TO 'mysql_rest_service_admin';
GRANT SELECT, INSERT ON `mysql_rest_service_metadata`.`mrs_user_hierarchy`
    TO 'mysql_rest_service_meta_provider';
GRANT SELECT ON `mysql_rest_service_metadata`.`mrs_user_hierarchy`
    TO 'mysql_rest_service_data_provider';

# `mysql_rest_service_metadata`.`mrs_user_hierarchy_type`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`mrs_user_hierarchy_type`
    TO 'mysql_rest_service_admin';
GRANT SELECT, INSERT ON `mysql_rest_service_metadata`.`mrs_user_hierarchy_type`
    TO 'mysql_rest_service_meta_provider';

# -----------------------------------------------------
# User Roles

# `mysql_rest_service_metadata`.`mrs_user_has_role`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`mrs_user_has_role`
    TO 'mysql_rest_service_admin';
GRANT SELECT, INSERT ON `mysql_rest_service_metadata`.`mrs_user_has_role`
    TO 'mysql_rest_service_meta_provider';

# `mysql_rest_service_metadata`.`mrs_role`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`mrs_role`
    TO 'mysql_rest_service_admin';
GRANT SELECT, INSERT ON `mysql_rest_service_metadata`.`mrs_role`
    TO 'mysql_rest_service_meta_provider';

# `mysql_rest_service_metadata`.`mrs_privilege`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`mrs_privilege`
    TO 'mysql_rest_service_admin';
GRANT SELECT, INSERT ON `mysql_rest_service_metadata`.`mrs_privilege`
    TO 'mysql_rest_service_meta_provider';

# -----------------------------------------------------
# User Group Management

# `mysql_rest_service_metadata`.`mrs_user_has_group`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`mrs_user_has_group`
    TO 'mysql_rest_service_admin';
GRANT SELECT, INSERT ON `mysql_rest_service_metadata`.`mrs_user_has_group`
    TO 'mysql_rest_service_meta_provider';

# `mysql_rest_service_metadata`.`mrs_user_group`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`mrs_user_group`
    TO 'mysql_rest_service_admin';
GRANT SELECT, INSERT ON `mysql_rest_service_metadata`.`mrs_user_group`
    TO 'mysql_rest_service_meta_provider';

# `mysql_rest_service_metadata`.`mrs_user_group_has_role`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`mrs_user_group_has_role`
    TO 'mysql_rest_service_admin';
GRANT SELECT, INSERT ON `mysql_rest_service_metadata`.`mrs_user_group_has_role`
    TO 'mysql_rest_service_meta_provider';

# `mysql_rest_service_metadata`.`mrs_group_hierarchy_type`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`mrs_group_hierarchy_type`
    TO 'mysql_rest_service_admin';
GRANT SELECT, INSERT ON `mysql_rest_service_metadata`.`mrs_group_hierarchy_type`
    TO 'mysql_rest_service_meta_provider';

# `mysql_rest_service_metadata`.`mrs_user_group_hierarchy`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`mrs_user_group_hierarchy`
    TO 'mysql_rest_service_admin';
GRANT SELECT, INSERT ON `mysql_rest_service_metadata`.`mrs_user_group_hierarchy`
    TO 'mysql_rest_service_meta_provider';
GRANT SELECT ON `mysql_rest_service_metadata`.`mrs_user_group_hierarchy`
    TO 'mysql_rest_service_data_provider';

# -----------------------------------------------------
# Router Management

# `mysql_rest_service_metadata`.`router`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`router`
    TO 'mysql_rest_service_admin';
GRANT SELECT, INSERT, UPDATE ON `mysql_rest_service_metadata`.`router`
    TO 'mysql_rest_service_meta_provider';
GRANT SELECT
    ON `mysql_rest_service_metadata`.`router`
    TO 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev';

# `mysql_rest_service_metadata`.`router_status`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`router_status`
    TO 'mysql_rest_service_admin';
GRANT SELECT, INSERT, UPDATE ON `mysql_rest_service_metadata`.`router_status`
    TO 'mysql_rest_service_meta_provider';
GRANT SELECT ON `mysql_rest_service_metadata`.`router_status`
    TO 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev';

# `mysql_rest_service_metadata`.`router_general_log`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`router_general_log`
    TO 'mysql_rest_service_admin';
GRANT INSERT ON `mysql_rest_service_metadata`.`router_general_log`
    TO 'mysql_rest_service_meta_provider';
GRANT SELECT ON `mysql_rest_service_metadata`.`router_general_log`
    TO 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev';

# `mysql_rest_service_metadata`.`router_session`
GRANT SELECT, INSERT, UPDATE, DELETE
    ON `mysql_rest_service_metadata`.`router_session`
    TO 'mysql_rest_service_admin';
GRANT SELECT, INSERT ON `mysql_rest_service_metadata`.`router_session`
    TO 'mysql_rest_service_meta_provider';
GRANT SELECT ON `mysql_rest_service_metadata`.`router_session`
    TO 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev';

# `mysql_rest_service_metadata`.`router_services`
GRANT SELECT ON `mysql_rest_service_metadata`.`router_services`
    TO 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev', 'mysql_rest_service_meta_provider';

# -----------------------------------------------------
# Procedures

# `mysql_rest_service_metadata`.`get_sequence_id`

GRANT EXECUTE ON FUNCTION `mysql_rest_service_metadata`.`get_sequence_id`
    TO 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev', 'mysql_rest_service_meta_provider', 'mysql_rest_service_data_provider';

# -----------------------------------------------------
# Views

# `mysql_rest_service_metadata`.`mrs_user_schema_version`

GRANT SELECT
  ON `mysql_rest_service_metadata`.`mrs_user_schema_version`
  TO 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev', 'mysql_rest_service_meta_provider';

# `mysql_rest_service_metadata`.`table_columns_with_references`

GRANT SELECT
  ON `mysql_rest_service_metadata`.`table_columns_with_references`
  TO 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev', 'mysql_rest_service_meta_provider';

# `mysql_rest_service_metadata`.`object_fields_with_references`

GRANT SELECT
  ON `mysql_rest_service_metadata`.`object_fields_with_references`
  TO 'mysql_rest_service_admin', 'mysql_rest_service_schema_admin', 'mysql_rest_service_dev', 'mysql_rest_service_meta_provider';

# -----------------------------------------------------
# Set the schema_version VIEW to the correct version at the very end

CREATE OR REPLACE SQL SECURITY INVOKER VIEW `mysql_rest_service_metadata`.`schema_version` (major, minor, patch) AS SELECT 3, 0, 2;

