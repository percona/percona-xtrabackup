/******************************************************
Copyright (c) 2023 Percona LLC and/or its affiliates.

interface containing map/set required for PXB

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

*******************************************************/
#ifndef XB_DICT_H
#define XB_DICT_H
#include <mysql.h>
#include <memory>
#include <unordered_set>
#include "include/db0err.h"
#include "include/dict0types.h"
// Forward declaration
namespace dd {
class Table;
}

namespace xb {
/* Tablespace identifier */
typedef uint32_t space_id_t;

/* We have different ways to deal with dictionary during backup and prepare.
Hence we separate namespaces. If there is common function between these two
they can be put in some common namespace */
namespace backup {
/* List of tablespace ids  from server dictionary. This is used
during backup to verify the IBDs we copy are valid. ie they have
an entry in dictionary. With this map, it is possible to detect
orphan IBDs in server data directory */
using dd_space_ids = std::unordered_set<space_id_t>;

/** Build space_id set using INFORMATION_SCHEMA.INNODB_TABLESPACES
@param[in] connection  MySQL connection handler
@param[out] dd_tabpespaces set containing all space_id present in DD */
std::shared_ptr<dd_space_ids> build_space_id_set(MYSQL *connection);
}  // namespace backup

namespace prepare {
/**
Dictionary design during prepare
--------------------------------

Dictionary is required for these purposes in PXB:
1. Prepare phase, to do rollback of transactions. Transactions use undo log
   and open tables based on "table_id" recorded in undo log
2. Export of tables (--export). For every file_per_table tablespace, a table
   object (dict_table_t) and a server table object (dd::Table) if table has
   instant columns.
3. --stats. For every table in tablespace, we iterate over all indexes and print
   free page ratio, cardinality etc. This also requies InnoDB table object
   (dict_table_t)

4. dynamic metadata apply etc require InnoDB dictionary table
mysql.dynamic_metadata to be be available

5. Duplicate SDI (opens mysql.tables and mysql.schemata)

Rollback Flow:
-------------
1. Load all tables(dict_table_t*) from mysql.ibd using SDI. These tables are not
closed because we dont want them to be evictable. Dictionary tables reside in
mysql.ibd

2. After dictionary tables are loaded, we scan mysql.indexes,
mysql.index_partitions to create a relation "table_id->space_id".

3. By default, we don't load any tables upfront (the old design). A table is
opened only on transaction rollback using table_id (dd_table_open_on_id()).
Since we know the corresponding space_id, we will use
SDI->dd::Table->dict_table_t conversion. Note that these tables are now
evictable and treated like the normal way. Normal as in the table->n_ref_count
is incremented on open, table can be used and close the table. Close operation
will decrement table->n_ref_count. Such tables are now evictable by background
threads. The only tables that are loaded "non-evictable" are tables from
mysql.ibd. These are special tables and are evicted only on shutdown/exit

Export flow:
------------

1. For every space_id, we load all tables in tablespace
(SDI->dd::Table->dict_table_t)

2. Since export works on file_per_table tablespaces, ideally there should be
only one dict_table_t from a single space_id

3. Exception is "partition tablespaces". The partition tablespaces, only of the
partition has all the SDI and the other partitions are 'empty'. Hence, from a
partition IBD, multiple dict_table_t objects are loaded.

4. For each dict_table_t object, we create a .cfg, .cfp file

Stats flow
----------
1. The flow is similar to steps mentioned in export
2. After a dict_table_t is available, iterate over dict_table_t->indexes to
calculate and print stats

Dynamic metadata flow
---------------------
1. All tables loaded as part of mysql.ibd contains the dictionary table
"mysql/innodb_dynamic_metadata"
2. dd_table_open_on_name() will always find the above table in cache because we
didn't close the tables in mysql.
3. srv_dict_recover_on_restart()

Duplicate SDI flow:
-------------------
1. Some of the IBDs from older server version have duplicate dd::Table objects
in IBD. This creates confusion to PXB as PXB doesn't know which 'dd::Table' to
use

2. To solve this problem, we scan mysql.schemata and mysql.tables dictionary
tables and figure out the right "sdi_id" for a given schema/tablename.

3. After tables from mysql.ibd are loaded, we look into the cache for
mysql.schemata and mysql.tables. Then scan every record from these tables to
create "sdi_id_map"

The tricky part (Partitions):
-----------------------------

Until now, we assumed for every table_id, we can find a space_id and just load
SDI from the space_id to get the table (SDI->dd::Table->dict_table_t).

Not always. We have a special case with partition IBDs. The SDI for all the
partition tables is not in each partition IBD. Instead, it is "one" of the
partition IBD So given a partition table_id, we cannot use it corresponding
"space_id" (This we got from dictionary mysql.index_partitions)

The problem with partitioned tablespaces is that, SDI exists in any ONE of the
partition IBDs. Not all. Lets say t1#p#p0.ibd, t1#p#p1.ibd, t1#p#p2.ibd.
All the information about p1 and p2 exists only in p0.ibd

From dictionary (mysql.index_partitions), for a given table_id, we know the
space_id (See table_id_space_map). So Lets say if table_id belongs to p2.ibd, we
cannot simply load the table from p2.ibd. We have to "somehow" find a way to
look into other partition IBDs.

Lets get into details. How do we find "all partitions" of table?

Answer: Scan mysql.index_partitions, we can get index_partition id and the
InnoDB space_id and also the table_id->space_id relation

table_id_space_map:
{table_id -> space_id}
{1075 -> 13} p0.ibd
{1076 -> 14} p1.ibd
{1077 -> 15} p2.ibd

part_id_spaces_map:
{partition_id -> space_id}

{296 -> 13
 296 -> 14
 296 -> 15
}

space_part_map
{space_id ->  partition_id }
{
13 : 296
14 : 296
15 : 296
}

Lets say, we are asked to load table for table_id 1077.
We look into table_id_space_map and figure out that the space_id
for 1077 is 15 (p2.ibd)

Next, check if this space belongs to a "partitioned table"
For this, we look into space_part_map. If an entry exists, it means
the space_id belongs to partitioned table.

We get the partition id. For space_id 15, the partition id is 296.

Given a partition_id 296, we can easily know all the partition IBs by using
part_id_spaces_map

For 296, we now know that the IBDs are 13, 14, 15.

We will start from lower bound(13) and start looking into all space_ids in this
group. (14, 15)
*/

/** This function uses dict_load_tables_from_space_id_low() with a callback
that loads all tables from dd::table into a vector
@param[in] space_id      InnoDB tablespace_id
@param[in] table_id      InnoDB table id. If this is zero, we load *all* tables
                         found in space_id
@param[in] thd           Server thread context (used for DD APIs)
@param[in] trx           InnoDB trx object (for using SDI APIs)
@return tuple <a,b>
a - DB_SUCCESS on success, other DB_ on errors
b - std::vector<dict_table_t*>, empty on errors */
using xb_dict_tuple = std::tuple<dberr_t, std::vector<dict_table_t *>>;
xb_dict_tuple dict_load_tables_from_space_id(space_id_t space_id,
                                             table_id_t table_id, THD *thd,
                                             trx_t *trx);

/** @return all tables (dict_table_t*) from a tablespace
@param[in] space_id InnoDB tablespace id */
xb_dict_tuple dict_load_from_spaces_sdi(space_id_t space_id);

/** @return true if table_id is found in dd map. This map
is created by scanning mysql.indexes and mysql.index_partitions
@param[in]  table_id InnoDB table id */
bool table_exists_in_dd(table_id_t table_id);

/** Load a specific table from space_id. This is used by InnoDB table opening
function dd_table_open_on_id()
@param[in] table_id InnoDB table id
@return tuple <a,b>
a - DB_SUCCESS on success, other DB_ on errors
b - std::vector<dict_table_t*>, empty on errors */
xb_dict_tuple dict_load_tables_using_table_id(table_id_t table_id);

/** Load all tables from mysql.ibd. This includes dictionary tables, system
tables
@return tuple <a,b>
a - DB_SUCCESS on success, other DB_ on errors
b - std::vector<dict_table_t*>, empty on errors */
xb_dict_tuple dict_load_from_mysql_ibd();

/** This function uses dict_load_tables_from_space_id_low() with a callback
that returns the dd::Table object to caller. We DONT convert dd::Table to
dict_table_t here
@param[in] space_id      InnoDB tablespace_id
@param[in] table_id      InnoDB table id. If this is zero, we load *all* tables
                         found in space_id
@return dd::Table object on success, else nullptr */
std::unique_ptr<dd::Table> get_dd_Table(space_id_t space_id,
                                        table_id_t table_id);

/** Clear all the maps created to handle dictionary during prepare */
void clear_dd_cache_maps();
}  // namespace prepare

}  // namespace xb
#endif
