#
# This grammar attempts to create realistic DML queries against the DBT-3 data
# set. It is derived from dbt3-joins.yy and is used in conjunction with the 
# DML Validator.
#
# * The test must operate in transactional environment due to the way the DML Validator
# operates
#
# * We use only non-unique, non-PK fields in the UPDATE list in order to prevent
# "duplicate key" errors
#

query_init:
	SET AUTOCOMMIT=OFF ; START TRANSACTION ;

query:
	START TRANSACTION ; update ;

update:
	update_r_n_s_ps_l_o_c | update_p_ps_s_n_r | update_p_ps_l_o_c_r_n_s | update_p_ps_s_l_o_c ;

# region -> nation -> supplier -> partsupp -> lineitem -> orders -> customer

update_r_n_s_ps_l_o_c:
	UPDATE ignore join_r_n_s_ps_l_o_c SET field_r_n_s_ps_l_o_c_nonunique = DEFAULT , field_r_n_s_ps_l_o_c_nonunique = DEFAULT WHERE where_r_n_s_ps_l_o_c ;

# part -> partsupp -> supplier -> nation -> region

update_p_ps_s_n_r:
	UPDATE ignore join_p_ps_s_n_r SET field_p_ps_s_n_r_nonunique = DEFAULT , field_p_ps_s_n_r_nonunique = DEFAULT WHERE where_p_ps_s_n_r ;

# part -> partsupp -> lineitem -> orders -> customer -> region -> nation -> supplier

update_p_ps_l_o_c_r_n_s:
	UPDATE ignore join_p_ps_l_o_c_r_n_s SET field_p_ps_l_o_c_r_n_s_nonunique = DEFAULT , field_p_ps_l_o_c_r_n_s_nonunique = DEFAULT WHERE where_p_ps_l_o_c_r_n_s ;

# part -> partsupp -> lineitem -> orders -> customer with currency fields only

update_p_ps_s_l_o_c:
	UPDATE ignore join_p_ps_s_l_o_c SET field_p_ps_s_l_o_c_nonunique = DEFAULT , field_p_ps_s_l_o_c_nonunique = DEFAULT WHERE where_p_ps_s_l_o_c ;

ignore:
	| | | | | | ;

asc_desc:
	| | | | | | ASC | DESC ;

order_by_1:
	| | ORDER BY 1 ;					# 30% of queries have ORDER BY on a single column

order_by_1_2:
	| | | | | | ORDER BY 1 | ORDER BY 2 | ORDER BY 1 , 2 ;	# 30% of queries have ORDER BY on two columns

join_r_n_s_ps_l_o_c:
	region join_type nation ON ( r_regionkey = n_regionkey ) join_type supplier ON ( s_nationkey = n_nationkey ) join_type partsupp ON ( s_suppkey = ps_suppkey ) join_type lineitem ON ( partsupp_lineitem_join_cond ) join_type orders ON ( l_orderkey = o_orderkey ) join_type customer ON ( o_custkey = c_custkey ) ;

join_p_ps_s_n_r:
	part join_type partsupp ON ( p_partkey = ps_partkey ) join_type supplier ON ( ps_suppkey = s_suppkey ) join_type nation ON ( s_nationkey = n_nationkey ) join_type region ON ( n_regionkey = r_regionkey ) ;

join_p_ps_l_o_c_r_n_s:
	part join_type partsupp ON ( p_partkey = ps_partkey ) join_type lineitem ON ( partsupp_lineitem_join_cond ) join_type orders ON ( l_orderkey = o_orderkey ) join_type customer ON ( o_custkey = c_custkey ) join_type nation ON ( c_nationkey = n_nationkey ) join_type supplier ON ( s_nationkey = n_nationkey ) join_type region ON ( n_regionkey = r_regionkey ) ;

join_p_ps_s_l_o_c:
	part join_type partsupp ON ( p_partkey = ps_partkey ) join_type supplier ON (s_suppkey = ps_suppkey) join_type lineitem ON ( partsupp_lineitem_join_cond ) join_type orders ON ( l_orderkey = o_orderkey ) join_type customer ON ( o_custkey = c_custkey ) ;
	
join_type:
	JOIN | LEFT JOIN | RIGHT JOIN ;

partsupp_lineitem_join_cond:
	ps_partkey = l_partkey AND ps_suppkey = l_suppkey |
	ps_partkey = l_partkey AND ps_suppkey = l_suppkey |
	ps_partkey = l_partkey | ps_suppkey = l_suppkey ;

lineitem_orders_join_cond:
	l_orderkey = o_orderkey | lineitem_date_field = o_orderdate ;

lineitem_date_field:
	l_shipDATE | l_commitDATE | l_receiptDATE ;

field_r_n_s_ps_l_o_c_nonunique:
	field_r_nonunique | field_n_nonunique | field_s_nonunique | field_ps_nonunique | field_l_nonunique | field_o_nonunique | field_c_nonunique ;

field_p_ps_s_n_r_nonunique:
	field_p_nonunique | field_ps_nonunique | field_s_nonunique | field_n_nonunique | field_r_nonunique ;

field_p_ps_l_o_c_r_n_s_nonunique:
	field_p_nonunique | field_ps_nonunique | field_l_nonunique | field_o_nonunique | field_c_nonunique | field_r_nonunique | field_n_nonunique | field_s_nonunique ;

field_p_ps_s_l_o_c_nonunique:
	field_p_nonunique | field_ps_nonunique | field_s_nonunique | field_l_nonunique | field_o_nonunique | field_c_nonunique ;

currency_field_p_ps_s_l_o_c:
	p_retailprice | ps_supplycost | l_extendedprice | o_totalprice | s_acctbal | c_acctbal ;

field_p:
	p_partkey;

field_s:
	s_suppkey | s_nationkey ;

field_ps:
	ps_partkey | ps_suppkey ;

field_l:
	l_orderkey | l_partkey | l_suppkey | l_linenumber | l_shipDATE | l_commitDATE | l_receiptDATE ;

field_o:
	o_orderkey | o_custkey ;

field_c:
	c_custkey | c_nationkey ;

field_n:
	n_nationkey ;

field_r:
	r_regionkey ;

#

field_p_nonunique:
	p_name | p_mfgr | p_brand | p_type | p_size | p_container | p_retailprice | p_comment ;

field_s_nonunique:
	s_name | s_address | s_nationkey | s_phone | s_acctbal | s_comment ;

field_ps_nonunique:
	ps_availqty | ps_supplycost | ps_comment ;

field_l_nonunique:
	l_partkey | l_suppkey | l_quantity | l_extendedprice | l_discount | l_tax | l_returnflag | l_linestatus | l_shipDATE | l_commitDATE | l_receiptDATE | l_shipinstruct | l_shipmode | l_comment ;

field_o_nonunique:
	o_custkey | o_orderstatus | o_totalprice | o_orderDATE | o_orderpriority | o_clerk | o_shippriority | o_comment ;

field_c_nonunique:
	c_name | c_address | c_nationkey | c_phone | c_acctbal | c_mktsegment | c_comment ;

field_n_nonunique:
	n_name | n_regionkey | n_comment ;

field_r_nonunique:
	r_name | r_comment ;

#

aggregate:
	COUNT( distinct | SUM( distinct | MIN( | MAX( ;

distinct:
	| | | DISTINCT ;

where_r_n_s_ps_l_o_c:
	cond_r_n_s_ps_l_o_c and_or cond_r_n_s_ps_l_o_c and_or cond_r_n_s_ps_l_o_c | where_r_n_s_ps_l_o_c and_or cond_r_n_s_ps_l_o_c ;
cond_r_n_s_ps_l_o_c:
	cond_r | cond_n | cond_s | cond_ps | cond_l | cond_o | cond_c | cond_l_o | cond_l_o | cond_s_c | cond_ps_l ;

where_p_ps_s_n_r:
	cond_p_ps_s_n_r and_or cond_p_ps_s_n_r and_or cond_p_ps_s_n_r | where_p_ps_s_n_r and_or cond_p_ps_s_n_r ;
cond_p_ps_s_n_r:
	cond_p | cond_ps | cond_s | cond_n | cond_r ;


where_p_ps_l_o_c_r_n_s:
	cond_p_ps_l_o_c_r_n_s and_or cond_p_ps_l_o_c_r_n_s and_or cond_p_ps_l_o_c_r_n_s | where_p_ps_l_o_c_r_n_s and_or cond_p_ps_l_o_c_r_n_s ;
cond_p_ps_l_o_c_r_n_s:
	cond_p | cond_ps | cond_l | cond_o | cond_c | cond_r | cond_n | cond_s ;

where_p_ps_s_l_o_c:
	cond_p_ps_s_l_o_c and_or cond_p_ps_s_l_o_c and_or cond_p_ps_s_l_o_c | where_p_ps_s_l_o_c and_or cond_p_ps_s_l_o_c ;

cond_p_ps_s_l_o_c:
	cond_p | cond_ps | cond_s | cond_l | cond_o | cond_c ;

currency_having:
	currency_having_item |
	currency_having_item and_or currency_having_item ;

currency_having_item:
	currency_having_field currency_clause ;

currency_having_field:
	currency1 | currency2 ;

and_or:
	AND | AND | AND | AND | OR ;

#
# Multi-table WHERE conditions
#

cond_l_o:
	l_extendedprice comp_op o_totalprice | lineitem_date_field comp_op o_orderdate ;

cond_ps_l:
	ps_availqty comp_op l_quantity | ps_supplycost comp_op l_extendedprice ;

cond_s_c:
	c_nationkey comp_op s_nationkey ;
	
#
# Per-table WHERE conditions
#

cond_p:
	p_partkey partkey_clause |
	p_retailprice currency_clause |
	p_comment comment_clause ;

cond_s:
	s_suppkey suppkey_clause |
	s_nationkey nationkey_clause |
	s_acctbal currency_clause |
	s_comment comment_clause ;

cond_ps:
	ps_partkey partkey_clause |
	ps_suppkey suppkey_clause |
	ps_supplycost currency_clause |
	ps_comment comment_clause ;

cond_l:
	l_linenumber linenumber_clause |
	l_shipDATE shipdate_clause |
	l_partkey partkey_clause |
	l_suppkey suppkey_clause |
	l_receiptDATE receiptdate_clause |
	l_orderkey orderkey_clause |
	l_quantity quantity_clause |
	l_commitDATE commitdate_clause |
	l_extendedprice currency_clause |
	l_comment comment_clause ;

cond_o:
	o_orderkey orderkey_clause |
	o_custkey custkey_clause |
	o_totalprice currency_clause |
	o_comment comment_clause ;

cond_c:
	c_custkey custkey_clause |
	c_acctbal currency_clause |
	c_comment comment_clause ;

cond_n:
	n_nationkey nationkey_clause |
	n_comment comment_clause ;

cond_r:
	r_regionkey regionkey_clause |
	r_comment comment_clause ;

#
# Per-column WHERE conditions
#

comp_op:
        = | = | = | = | != | > | >= | < | <= | <> ;

not:
	| | | | | | | | | NOT ;

shipdate_clause:
	comp_op any_date |
	not IN ( date_list ) |
	date_between ;

date_list:
	date_item , date_item |
	date_list , date_item ;

date_item:
	any_date | any_date | any_date | any_date | any_date |
	any_date | any_date | any_date | any_date | any_date |
	any_date | any_date | any_date | any_date | any_date |
	any_date | any_date | any_date | any_date | any_date |
	'1992-01-08' | '1998-11-27' ;

date_between:
	BETWEEN date_item AND date_item |
	between_two_dates_in_a_year |
	between_two_dates_in_a_month |
	within_a_month ;

day_month_year:
	DAY | MONTH | YEAR ;

any_date:
	{ sprintf("'%04d-%02d-%02d'", $prng->uint16(1992,1998), $prng->uint16(1,12), $prng->uint16(1,28)) } ;

between_two_dates_in_a_year:
	{ my $year = $prng->uint16(1992,1998); return sprintf("BETWEEN '%04d-%02d-%02d' AND '%04d-%02d-%02d'", $year, $prng->uint16(1,12), $prng->uint16(1,28), $year, $prng->uint16(1,12), $prng->uint16(1,28)) } ;

between_two_dates_in_a_month:
	{ my $year = $prng->uint16(1992,1998); my $month = $prng->uint16(1,12); return sprintf("BETWEEN '%04d-%02d-%02d' AND '%04d-%02d-%02d'", $year, $month, $prng->uint16(1,28), $year, $month, $prng->uint16(1,28)) } ;

within_a_month:
	{ my $year = $prng->uint16(1992,1998); my $month = $prng->uint16(1,12); return sprintf("BETWEEN '%04d-%02d-01' AND '%04d-%02d-29'", $year, $month, $year, $month) } ;

# LINENUMBER

linenumber_clause:
	comp_op linenumber_item |
	not IN ( linenumber_list ) |
	BETWEEN linenumber_item AND linenumber_item + linenumber_range ;

linenumber_list:
	linenumber_item , linenumber_item |
	linenumber_item , linenumber_list ;

linenumber_item:
	_digit; 

linenumber_range:
	_digit ;

# PARTKEY

partkey_clause:
	comp_op partkey_item |
	not IN ( partkey_list ) |
	BETWEEN partkey_item AND partkey_item + partkey_range ;

partkey_list:
	partkey_item , partkey_item |
	partkey_item , partkey_list ;

partkey_range:
	_digit | _tinyint_unsigned;

partkey_item:
	_tinyint_unsigned  | _tinyint_unsigned | _tinyint_unsigned | _tinyint_unsigned | _tinyint_unsigned |
	_tinyint_unsigned  | _tinyint_unsigned | _tinyint_unsigned | _tinyint_unsigned | _tinyint_unsigned |
	_tinyint_unsigned  | _tinyint_unsigned | _tinyint_unsigned | _tinyint_unsigned | _tinyint_unsigned |
	_tinyint_unsigned  | _tinyint_unsigned | _tinyint_unsigned | _tinyint_unsigned | _tinyint_unsigned |
	_digit | 200 | 0 ;

# SUPPKEY

suppkey_clause:
	comp_op suppkey_item |
	not IN ( suppkey_list ) |
	BETWEEN suppkey_item AND suppkey_item + _digit ;

suppkey_item:
	_digit | 10 ;

suppkey_list:
	suppkey_item , suppkey_item |
	suppkey_item , suppkey_list ;

# RECEPITDATE

receiptdate_clause:
	comp_op any_date |
	not IN ( date_list ) |
	date_between ;

# COMMITDATE

commitdate_clause:
	comp_op any_date |
	not IN ( date_list ) |
	date_between ;

# ORDERKEY

orderkey_clause:
	comp_op orderkey_item |
	not IN ( orderkey_list ) |
	BETWEEN orderkey_item AND orderkey_item + orderkey_range ;

orderkey_item:
	_tinyint_unsigned | { $prng->uint16(1,1500) } ;

orderkey_list:
	orderkey_item , orderkey_item |
	orderkey_item , orderkey_list ;

orderkey_range:
	_digit | _tinyint_unsigned ;

# QUANTITY

quantity_clause:
	comp_op quantity_item |
	not IN ( quantity_list ) |
	BETWEEN quantity_item AND quantity_item + quantity_range ;

quantity_list:
	quantity_item , quantity_item |
	quantity_item , quantity_list ;

quantity_item:
	_digit  | { $prng->uint16(1,50) } ;

quantity_range:
	_digit ;

# CUSTKEY

custkey_clause:
	comp_op custkey_item |
	not IN ( custkey_list ) |
	BETWEEN custkey_item AND custkey_item + custkey_range ;

custkey_item:
	_tinyint_unsigned | { $prng->uint16(1,150) } ;

custkey_list:
	custkey_item , custkey_item |
	custkey_item , custkey_list ;

custkey_range:
	_digit | _tinyint_unsigned ;

# NATIONKEY 

nationkey_clause:
	comp_op nationkey_item |
	not IN ( nationkey_list ) |
	BETWEEN nationkey_item AND nationkey_item + nationkey_range ;

nationkey_item:
	_digit | { $prng->uint16(0,24) } ;

nationkey_list:
	nationkey_item , nationkey_item |
	nationkey_item , nationkey_list ;

nationkey_range:
	_digit | _tinyint_unsigned ;

# REGIONKEY 

regionkey_clause:
	comp_op regionkey_item |
	not IN ( regionkey_list ) |
	BETWEEN regionkey_item AND regionkey_item + regionkey_range ;

regionkey_item:
	1 | 2 | 3 | 4 ;

regionkey_list:
	regionkey_item , regionkey_item |
	regionkey_item , regionkey_list ;

regionkey_range:
	1 | 2 | 3 | 4 ;

# COMMENT

comment_clause:
	IS NOT NULL | IS NOT NULL | IS NOT NULL |
	comp_op _varchar(1) |
	comment_not LIKE CONCAT( comment_count , '%' ) |
	BETWEEN _varchar(1) AND _varchar(1) ;

comment_not:
	NOT | NOT | NOT | ;

comment_count:
	_varchar(1) | _varchar(1) |  _varchar(1) | _varchar(1) | _varchar(2) ;

# CURRENCIES

currency_clause:
	comp_op currency_item |
	BETWEEN currency_item AND currency_item + currency_range ;

currency_item:
	_digit | _tinyint_unsigned | _tinyint_unsigned | _tinyint_unsigned | _mediumint_unsigned ;

currency_range:
	_tinyint_unsigned ;
