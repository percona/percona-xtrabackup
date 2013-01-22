DROP TABLE IF EXISTS `test`;
CREATE TABLE `test` (
  `a` int(11) NOT NULL PRIMARY KEY,
  `number` int(11) DEFAULT NULL,
  KEY(number)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
