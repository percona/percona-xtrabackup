DELIMITER $$;
CREATE PROCEDURE return_alpha_from_10k_uuids()
SQL SECURITY INVOKER
BEGIN
  DECLARE STRLEN int;
  DECLARE p1 int default 10;
  DECLARE p2 int default 0;
  DECLARE retval varchar(50) default '';
  DECLARE myuuid char(50) default '';

  label1: WHILE p1 > 0 DO
    SET p1 = p1 - 1;
    select uuid() into myuuid;
    set retval='';
    set p2 = 0;
    label2: WHILE p2 < 36 DO
      set p2 = p2 + 1;
      set retval = concat(retval,if(substring(myuuid, p2, 1) REGEXP '[[:alpha:]]', substring(myuuid, p2, 1), ''));
    END WHILE label2;
    # select retval;
  END WHILE label1;
END$$
DELIMITER ;$$

call return_alpha_from_10k_uuids();

DROP PROCEDURE return_alpha_from_10k_uuids;
