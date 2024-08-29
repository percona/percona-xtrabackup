# Index of files created by Percona XtraBackup


* Information related to the backup and the server

<table>
    <tr>
        <th>File Name</th>
        <th>Description</th>
    </tr>
    <tr>
        <td><code><span class="pre">backup-my.cnf</span></code></td>
        <td> This file contains information to start the mini instance 
of InnoDB during the <code><span 
class="pre">--prepare</span></code>. 
This 
**not** a 
backup of the original <code><span class="pre">my.cnf</span></code>. The 
InnoDB configuration is 
read from 
the file <code><span class="pre">backup-my.cnf</span></code> created by 
<b>xtrabackup</b> when the backup was made. The <code><span 
class="pre">--prepare</span></code> uses InnoDB 
configuration from <code><span class="pre">backup-my.cnf</span></code> 
by default, or from <code><span 
class="pre">--defaults-file</span></code>, if specified. The InnoDB's configuration in this context means server variables that affect dataformat, i.e. <code><span class="pre">innodb_page_size</span></code> option, <code><span class="pre">innodb_log_block_size</span></code>, etc. Location-related variables, like <code><span class="pre">innodb_log_group_home_dir</span></code> or <code><span class="pre">innodb_data_file_path</span></code> are always ignored by <code><span class="pre">--prepare</span></code>, so preparing a backup always works with data files from the back directory, rather than any external ones.    </td></tr>
    <tr>
        <td><code><span class="pre">xtrabackup_checkpoints</span></code
></td><td><p>The type of the backup (for example, full or incremental), 
its state (for example, prepared) and the LSN range contained in it. 
This information is used for incremental backups. Example of the 
<code><span class="pre">xtrabackup_checkpoints</span></code> after 
taking a full backup:</p><div class="highlight-text"><div 
class="highlight"><pre><span></span>backup_type = full-backuped
from lsn= 0
to_lsn = 15188961605
last_lsn = 15188961605
</pre></div>
</div>
Example of the <code><span 
class="pre">xtrabackup_checkpoints</span></code> after taking an 
incremental backup:</p>
<div class="last highlight-text"><div class="highlight"><pre><span></span>backup_type = incremental
from_lsn = 15188961605
to_lsn = 15189350111
last_lsn = 15189350111
</pre></div>
</div></td></tr>
<tr>
       <td> <code><span 
class="pre">xtrabackup_binlog_info</span></code
></td>
        <td><p>The binary log file used by the server and its position at the moment of the backup. A result of the following query:</p>
        <div class="first last highlight-mysql"><div class="highlight"><pre><span></span><span class="k">SELECT</span> <span class="n">server_uuid</span><span class="p">,</span> <span class="n">local</span><span class="p">,</span> <span class="n">replication</span><span class="p">,</span> <span class="n">storage_engines</span> <span class="k">FROM</span> <span class="n">performance_schema</span><span class="p">.</span><span class="n">log_status</span><span class="p">;</span>
</pre></div>
</div>

</td></tr>
<tr><td><code><span 
class="pre">xtrabackup_binlog</span></code
></td>
<td>The <b>xtrabackup</b> binary used in the process.
</td></tr>
<tr><td><code><span 
class="pre">xtrabackup_logfile</span></code
></td>
<td>Contains data needed for running the: <code><span 
class="pre">--prepare</span></code>.
>     The bigger this file is the <code><span 
class="pre">--prepare</span></code> process
>     will take longer to finish.
</td></tr>
<tr><td><code><span 
class="pre">&lt;table_name&gt;.delta.meta</span></code
></td>
<td><p>This file is going to be created when performing the incremental 
backup.
>     It contains the per-table delta metadata: page size, size of compressed
>     page (if the value is 0 it means the tablespace isn’t compressed) and
>     space id. Example of this file:</p>
<div class="last highlight-text"><div class="highlight"><pre><span></span>page_size = 16384
zip_size = 0
space_id = 0
</pre></div>
</div>
</td></tr>
</table>

* Information related to the replication environment (if using the
`--slave-info` option):</br></br>

  `xtrabackup_slave_info`</br>
     The `CHANGE MASTER` statement needed for setting up a replica.


* Information related to the *Galera* and *Percona XtraDB Cluster* (if 
  using the `--galera-info` option):</br></br>

  `xtrabackup_galera_info`</br>
Contains the values of `wsrep_local_state_uuid` and`wsrep_last_committed` status variables
