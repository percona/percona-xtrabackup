#!/usr/bin/env perl
#
# A script for making backups of InnoDB and MyISAM tables, indexes and .frm
# files.
#
# Copyright 2003, 2009 Innobase Oy. All Rights Reserved.
# Copyright Percona LLC and/or its affiliates, 2009-2014.  All Rights Reserved.
#

use warnings FATAL => 'all';
use strict;
use Getopt::Long;
use File::Spec;
use Pod::Usage qw(pod2usage);
use POSIX "strftime";
use POSIX ":sys_wait_h";
use FileHandle;
use File::Basename;
use File::Temp;
use File::Find;
use File::Copy;
use File::Path;
use English qw(-no_match_vars);
use Time::HiRes qw(usleep);
use Carp qw(longmess);
use Cwd qw(realpath);

# version of this script
my $innobackup_version = '1.5.1-xtrabackup';
my $innobackup_script = basename($0);

# copyright notice
my $copyright_notice =
"InnoDB Backup Utility v${innobackup_version}; Copyright 2003, 2009 Innobase Oy
and Percona LLC and/or its affiliates 2009-2013.  All Rights Reserved.

This software is published under
the GNU GENERAL PUBLIC LICENSE Version 2, June 1991.

";

# required Perl version (5.005)
my @required_perl_version = (5, 0, 5);
my $required_perl_version_old_style = 5.005;

# check existence of DBD::mysql module
eval {
    require DBD::mysql;
};
my $dbd_mysql_installed = $EVAL_ERROR ? 0 : 1;

# force flush after every write and print
$| = 1;

# disable nlink count optimization, see File::Find documentation for details
$File::Find::dont_use_nlink=1;

######################################################################
# modifiable parameters
######################################################################

# maximum number of files in a database directory which are
# separately printed when a backup is made
my $backup_file_print_limit = 9;

# default compression level (this is an argument to ibbackup)
my $default_compression_level = 1;

######################################################################
# end of modifiable parameters
######################################################################

#######################
# Server capabilities #
#######################
my $have_changed_page_bitmaps = 0;
my $have_backup_locks = 0;
my $have_galera_enabled = 0;
my $have_flush_engine_logs = 0;
my $have_multi_threaded_slave = 0;
my $have_gtid_slave = 0;
my $have_lock_wait_timeout = 0;

# command line options
my $option_help = '';
my $option_version = '';
my $option_apply_log = '';
my $option_redo_only = '';
my $option_copy_back = '';
my $option_move_back = '';
my $option_include = '';
my $option_databases = '';
my $option_tables_file = '';
my $option_throttle = '';
my $option_sleep = '';
my $option_compress = 999;
my $option_decompress = '';
my $option_compress_threads = 1;
my $option_compress_chunk_size = '';
my $option_encrypt = '';
my $option_decrypt = '';
my $option_encrypt_key = '';
my $option_encrypt_key_file = '';
my $encrypt_cmd = '';
my $option_encrypt_threads = 1;
my $option_encrypt_chunk_size = '';
my $option_export = '';
my $option_use_memory = '';
my $option_mysql_password = '';
my $option_mysql_user = '';
my $option_mysql_port = '';
my $option_mysql_socket = '';
my $option_mysql_host = '';
my $option_defaults_group = 'mysqld';
my $option_no_timestamp = '';
my $option_slave_info = '';
my $option_galera_info = '';
my $option_no_lock = '';
my $option_ibbackup_binary = 'xtrabackup';
my $option_log_copy_interval = 0;

my $option_defaults_file = '';
my $option_defaults_extra_file = '';
my $option_incremental = '';
my $option_incremental_basedir = '';
my $option_incremental_dir = '';
my $option_incremental_force_scan = 0;
my $option_incremental_lsn = '';
my $option_extra_lsndir = '';
my $option_rsync = '';
my $option_stream = '';
my $stream_cmd = '';
my $option_tmpdir = '';

my $option_tar4ibd = '';
my $option_scp_opt = '-Cp -c arcfour';
my $option_ssh_opt = '';

my $option_parallel = '';

my $option_safe_slave_backup = '';
my $option_safe_slave_backup_timeout = 300;

my $option_close_files = '';
my $option_compact = '';
my $option_rebuild_indexes = '';
my $option_rebuild_threads = 0;

my $option_debug_sleep_before_unlock = '';

my $option_version_check = '1';

my $option_force_non_empty_dirs = '';

my %mysql;
my $option_backup = '';

my $option_history;
my $option_incremental_history_name = '';
my $option_incremental_history_uuid = '';

# name of the my.cnf configuration file
#my $config_file = '';

# root of the backup directory
my $backup_root = '';

# backup directory pathname
my $backup_dir = '';

# directory where innobackupex aux files are cretead
my $work_dir;

# home directory of innoDB log files
my $innodb_log_group_home_dir = '';

# backup my.cnf file
my $backup_config_file = '';

# whether slave SQL thread is running when wait_for_safe_slave() is called
my $sql_thread_started = 0;

# options from the options file
my %config;

# options from the backup options file
#my %backup_config;

# list of databases to be included in a backup
my %databases_list;

# list of tables to be included in a backup
my %table_list;

# prefix for output lines
my $prefix = "$innobackup_script:";

# mysql server version string
my $mysql_server_version = '';

# name of the file where binlog position info is written
my $binlog_info;

# name of the file where galera position info is written
my $galera_info;

# name of the file where slave info is written
my $slave_info;

# name of the file where version and backup history is written 
my $backup_history;

# mysql binlog position as given by "SHOW MASTER STATUS" command
my $mysql_binlog_position = '';

# mysql master's binlog position as given by "SHOW SLAVE STATUS" command
# run on a slave server
my $mysql_slave_position = '';

# process id if the script itself
my $innobackupex_pid = $$;

# process id of ibbackup program (runs as a child process of this script)
my $ibbackup_pid = '';

# process id of long queries killer
my $query_killer_pid;

# set kill long queries timeout in seconds
my $option_kill_long_queries_timeout = 0;

# waiting for an appropriate time to start FTWRL timeout
my $option_lock_wait_timeout = 0;

# how old should be query to be waited for
my $option_lock_wait_threshold = 60;

# which type of queries we are waiting for during the pre-FTWRL phase
# possible values are "update" and "all"
my $option_lock_wait_query_type = "all";

# which type of queries wa are waiting when clearing the way for FTWRL
# by killing; possible values are "select" and "all"
my $option_kill_long_query_type = "all";

# a counter for numbering mysql connection checks
my $hello_id = 0;

# escape sequences for options files
my %option_value_escapes = ('b' => "\b",
                            't' => "\t",
                            'n' => "\n",
                            'r' => "\r",
                            "\\" => "\\",
                            's' => ' ');

# signal that is sent to child processes when they are killed
my $kill_signal = 9;

# current local time
my $now;

# incremental backup base directory
my $incremental_basedir = '';

my $src_name;
my $dst_name;
my $win = ($^O eq 'MSWin32' ? 1 : 0);
my $CP_CMD = ($win eq 1 ? "copy /Y" : "cp");
my $xtrabackup_pid_file = 'xtrabackup_pid';
my %rsync_files_hash;
my %processed_files;

my $xb_fn_suspended_at_start = "/xtrabackup_suspended_1";
my $xb_fn_suspended_at_end = "/xtrabackup_suspended_2";
my $xb_fn_log_copied = "/xtrabackup_log_copied";
my @xb_suspend_files = ($xb_fn_suspended_at_start, $xb_fn_suspended_at_end,
                        $xb_fn_log_copied);

my $copy_dir_src;
my $copy_dir_dst;
my $cleanup_dir_dst;
my $process_dir_exclude_regexp;
my $copy_dir_overwrite;
my $copy_dir_resolve_isl;

# for history on server record
my $history_tool_command = "@ARGV";
my $history_start_time = time();
my $history_lock_time = 0;
my $history_delete_checkpoints = 0;

# ###########################################################################
# HTTPMicro package
# This package is a copy without comments from the original.  The original
# with comments and its test file can be found in the Bazaar repository at,
#   lib/HTTPMicro.pm
#   t/lib/HTTPMicro.t
# See https://launchpad.net/percona-toolkit for more information.
# ###########################################################################
{

package HTTPMicro;
BEGIN {
  $HTTPMicro::VERSION = '0.001';
}
use strict;
use warnings;

use Carp ();


my @attributes;
BEGIN {
    @attributes = qw(agent timeout);
    no strict 'refs';
    for my $accessor ( @attributes ) {
        *{$accessor} = sub {
            @_ > 1 ? $_[0]->{$accessor} = $_[1] : $_[0]->{$accessor};
        };
    }
}

sub new {
    my($class, %args) = @_;
    (my $agent = $class) =~ s{::}{-}g;
    my $self = {
        agent        => $agent . "/" . ($class->VERSION || 0),
        timeout      => 60,
    };
    for my $key ( @attributes ) {
        $self->{$key} = $args{$key} if exists $args{$key}
    }
    return bless $self, $class;
}

my %DefaultPort = (
    http => 80,
    https => 443,
);

sub request {
    my ($self, $method, $url, $args) = @_;
    @_ == 3 || (@_ == 4 && ref $args eq 'HASH')
      or Carp::croak(q/Usage: $http->request(METHOD, URL, [HASHREF])/);
    $args ||= {}; # we keep some state in this during _request

    my $response;
    for ( 0 .. 1 ) {
        $response = eval { $self->_request($method, $url, $args) };
        last unless $@ && $method eq 'GET'
            && $@ =~ m{^(?:Socket closed|Unexpected end)};
    }

    if (my $e = "$@") {
        $response = {
            success => q{},
            status  => 599,
            reason  => 'Internal Exception',
            content => $e,
            headers => {
                'content-type'   => 'text/plain',
                'content-length' => length $e,
            }
        };
    }
    return $response;
}

sub _request {
    my ($self, $method, $url, $args) = @_;

    my ($scheme, $host, $port, $path_query) = $self->_split_url($url);

    my $request = {
        method    => $method,
        scheme    => $scheme,
        host_port => ($port == $DefaultPort{$scheme} ? $host : "$host:$port"),
        uri       => $path_query,
        headers   => {},
    };

    my $handle  = HTTPMicro::Handle->new(timeout => $self->{timeout});

    $handle->connect($scheme, $host, $port);

    $self->_prepare_headers_and_cb($request, $args);
    $handle->write_request_header(@{$request}{qw/method uri headers/});
    $handle->write_content_body($request) if $request->{content};

    my $response;
    do { $response = $handle->read_response_header }
        until (substr($response->{status},0,1) ne '1');

    if (!($method eq 'HEAD' || $response->{status} =~ /^[23]04/)) {
        $response->{content} = '';
        $handle->read_content_body(sub { $_[1]->{content} .= $_[0] }, $response);
    }

    $handle->close;
    $response->{success} = substr($response->{status},0,1) eq '2';
    return $response;
}

sub _prepare_headers_and_cb {
    my ($self, $request, $args) = @_;

    for ($args->{headers}) {
        next unless defined;
        while (my ($k, $v) = each %$_) {
            $request->{headers}{lc $k} = $v;
        }
    }
    $request->{headers}{'host'}         = $request->{host_port};
    $request->{headers}{'connection'}   = "close";
    $request->{headers}{'user-agent'} ||= $self->{agent};

    if (defined $args->{content}) {
        $request->{headers}{'content-type'} ||= "application/octet-stream";
        utf8::downgrade($args->{content}, 1)
            or Carp::croak(q/Wide character in request message body/);
        $request->{headers}{'content-length'} = length $args->{content};
        $request->{content} = $args->{content};
    }
    return;
}

sub _split_url {
    my $url = pop;

    my ($scheme, $authority, $path_query) = $url =~ m<\A([^:/?#]+)://([^/?#]*)([^#]*)>
      or Carp::croak(qq/Cannot parse URL: '$url'/);

    $scheme     = lc $scheme;
    $path_query = "/$path_query" unless $path_query =~ m<\A/>;

    my $host = (length($authority)) ? lc $authority : 'localhost';
       $host =~ s/\A[^@]*@//;   # userinfo
    my $port = do {
       $host =~ s/:([0-9]*)\z// && length $1
         ? $1
         : $DefaultPort{$scheme}
    };

    return ($scheme, $host, $port, $path_query);
}

package
    HTTPMicro::Handle; # hide from PAUSE/indexers
use strict;
use warnings;

use Carp       qw[croak];
use Errno      qw[EINTR EPIPE];
use IO::Socket qw[SOCK_STREAM];

sub BUFSIZE () { 32768 }

my $Printable = sub {
    local $_ = shift;
    s/\r/\\r/g;
    s/\n/\\n/g;
    s/\t/\\t/g;
    s/([^\x20-\x7E])/sprintf('\\x%.2X', ord($1))/ge;
    $_;
};

sub new {
    my ($class, %args) = @_;
    return bless {
        rbuf             => '',
        timeout          => 60,
        max_line_size    => 16384,
        %args
    }, $class;
}

my $ssl_verify_args = {
    check_cn => "when_only",
    wildcards_in_alt => "anywhere",
    wildcards_in_cn => "anywhere"
};

sub connect {
    @_ == 4 || croak(q/Usage: $handle->connect(scheme, host, port)/);
    my ($self, $scheme, $host, $port) = @_;

    if ( $scheme eq 'https' ) {
        eval "require IO::Socket::SSL"
            unless exists $INC{'IO/Socket/SSL.pm'};
        croak(qq/IO::Socket::SSL must be installed for https support\n/)
            unless $INC{'IO/Socket/SSL.pm'};
    }
    elsif ( $scheme ne 'http' ) {
      croak(qq/Unsupported URL scheme '$scheme'\n/);
    }

    $self->{fh} = 'IO::Socket::INET'->new(
        PeerHost  => $host,
        PeerPort  => $port,
        Proto     => 'tcp',
        Type      => SOCK_STREAM,
        Timeout   => $self->{timeout}
    ) or croak(qq/Could not connect to '$host:$port': $@/);

    binmode($self->{fh})
      or croak(qq/Could not binmode() socket: '$!'/);

    if ( $scheme eq 'https') {
        IO::Socket::SSL->start_SSL($self->{fh});
        ref($self->{fh}) eq 'IO::Socket::SSL'
            or die(qq/SSL connection failed for $host\n/);
        if ( $self->{fh}->can("verify_hostname") ) {
            $self->{fh}->verify_hostname( $host, $ssl_verify_args );
        }
        else {
         my $fh = $self->{fh};
         _verify_hostname_of_cert($host, _peer_certificate($fh), $ssl_verify_args)
               or die(qq/SSL certificate not valid for $host\n/);
         }
    }
      
    $self->{host} = $host;
    $self->{port} = $port;

    return $self;
}

sub close {
    @_ == 1 || croak(q/Usage: $handle->close()/);
    my ($self) = @_;
    CORE::close($self->{fh})
      or croak(qq/Could not close socket: '$!'/);
}

sub write {
    @_ == 2 || croak(q/Usage: $handle->write(buf)/);
    my ($self, $buf) = @_;

    my $len = length $buf;
    my $off = 0;

    local $SIG{PIPE} = 'IGNORE';

    while () {
        $self->can_write
          or croak(q/Timed out while waiting for socket to become ready for writing/);
        my $r = syswrite($self->{fh}, $buf, $len, $off);
        if (defined $r) {
            $len -= $r;
            $off += $r;
            last unless $len > 0;
        }
        elsif ($! == EPIPE) {
            croak(qq/Socket closed by remote server: $!/);
        }
        elsif ($! != EINTR) {
            croak(qq/Could not write to socket: '$!'/);
        }
    }
    return $off;
}

sub read {
    @_ == 2 || @_ == 3 || croak(q/Usage: $handle->read(len)/);
    my ($self, $len) = @_;

    my $buf  = '';
    my $got = length $self->{rbuf};

    if ($got) {
        my $take = ($got < $len) ? $got : $len;
        $buf  = substr($self->{rbuf}, 0, $take, '');
        $len -= $take;
    }

    while ($len > 0) {
        $self->can_read
          or croak(q/Timed out while waiting for socket to become ready for reading/);
        my $r = sysread($self->{fh}, $buf, $len, length $buf);
        if (defined $r) {
            last unless $r;
            $len -= $r;
        }
        elsif ($! != EINTR) {
            croak(qq/Could not read from socket: '$!'/);
        }
    }
    if ($len) {
        croak(q/Unexpected end of stream/);
    }
    return $buf;
}

sub readline {
    @_ == 1 || croak(q/Usage: $handle->readline()/);
    my ($self) = @_;

    while () {
        if ($self->{rbuf} =~ s/\A ([^\x0D\x0A]* \x0D?\x0A)//x) {
            return $1;
        }
        $self->can_read
          or croak(q/Timed out while waiting for socket to become ready for reading/);
        my $r = sysread($self->{fh}, $self->{rbuf}, BUFSIZE, length $self->{rbuf});
        if (defined $r) {
            last unless $r;
        }
        elsif ($! != EINTR) {
            croak(qq/Could not read from socket: '$!'/);
        }
    }
    croak(q/Unexpected end of stream while looking for line/);
}

sub read_header_lines {
    @_ == 1 || @_ == 2 || croak(q/Usage: $handle->read_header_lines([headers])/);
    my ($self, $headers) = @_;
    $headers ||= {};
    my $lines   = 0;
    my $val;

    while () {
         my $line = $self->readline;

         if ($line =~ /\A ([^\x00-\x1F\x7F:]+) : [\x09\x20]* ([^\x0D\x0A]*)/x) {
             my ($field_name) = lc $1;
             $val = \($headers->{$field_name} = $2);
         }
         elsif ($line =~ /\A [\x09\x20]+ ([^\x0D\x0A]*)/x) {
             $val
               or croak(q/Unexpected header continuation line/);
             next unless length $1;
             $$val .= ' ' if length $$val;
             $$val .= $1;
         }
         elsif ($line =~ /\A \x0D?\x0A \z/x) {
            last;
         }
         else {
            croak(q/Malformed header line: / . $Printable->($line));
         }
    }
    return $headers;
}

sub write_header_lines {
    (@_ == 2 && ref $_[1] eq 'HASH') || croak(q/Usage: $handle->write_header_lines(headers)/);
    my($self, $headers) = @_;

    my $buf = '';
    while (my ($k, $v) = each %$headers) {
        my $field_name = lc $k;
         $field_name =~ /\A [\x21\x23-\x27\x2A\x2B\x2D\x2E\x30-\x39\x41-\x5A\x5E-\x7A\x7C\x7E]+ \z/x
            or croak(q/Invalid HTTP header field name: / . $Printable->($field_name));
         $field_name =~ s/\b(\w)/\u$1/g;
         $buf .= "$field_name: $v\x0D\x0A";
    }
    $buf .= "\x0D\x0A";
    return $self->write($buf);
}

sub read_content_body {
    @_ == 3 || @_ == 4 || croak(q/Usage: $handle->read_content_body(callback, response, [read_length])/);
    my ($self, $cb, $response, $len) = @_;
    $len ||= $response->{headers}{'content-length'};

    croak("No content-length in the returned response, and this "
        . "UA doesn't implement chunking") unless defined $len;

    while ($len > 0) {
        my $read = ($len > BUFSIZE) ? BUFSIZE : $len;
        $cb->($self->read($read), $response);
        $len -= $read;
    }

    return;
}

sub write_content_body {
    @_ == 2 || croak(q/Usage: $handle->write_content_body(request)/);
    my ($self, $request) = @_;
    my ($len, $content_length) = (0, $request->{headers}{'content-length'});

    $len += $self->write($request->{content});

    $len == $content_length
      or croak(qq/Content-Length missmatch (got: $len expected: $content_length)/);

    return $len;
}

sub read_response_header {
    @_ == 1 || croak(q/Usage: $handle->read_response_header()/);
    my ($self) = @_;

    my $line = $self->readline;

    $line =~ /\A (HTTP\/(0*\d+\.0*\d+)) [\x09\x20]+ ([0-9]{3}) [\x09\x20]+ ([^\x0D\x0A]*) \x0D?\x0A/x
      or croak(q/Malformed Status-Line: / . $Printable->($line));

    my ($protocol, $version, $status, $reason) = ($1, $2, $3, $4);

    return {
        status   => $status,
        reason   => $reason,
        headers  => $self->read_header_lines,
        protocol => $protocol,
    };
}

sub write_request_header {
    @_ == 4 || croak(q/Usage: $handle->write_request_header(method, request_uri, headers)/);
    my ($self, $method, $request_uri, $headers) = @_;

    return $self->write("$method $request_uri HTTP/1.1\x0D\x0A")
         + $self->write_header_lines($headers);
}

sub _do_timeout {
    my ($self, $type, $timeout) = @_;
    $timeout = $self->{timeout}
        unless defined $timeout && $timeout >= 0;

    my $fd = fileno $self->{fh};
    defined $fd && $fd >= 0
      or croak(q/select(2): 'Bad file descriptor'/);

    my $initial = time;
    my $pending = $timeout;
    my $nfound;

    vec(my $fdset = '', $fd, 1) = 1;

    while () {
        $nfound = ($type eq 'read')
            ? select($fdset, undef, undef, $pending)
            : select(undef, $fdset, undef, $pending) ;
        if ($nfound == -1) {
            $! == EINTR
              or croak(qq/select(2): '$!'/);
            redo if !$timeout || ($pending = $timeout - (time - $initial)) > 0;
            $nfound = 0;
        }
        last;
    }
    $! = 0;
    return $nfound;
}

sub can_read {
    @_ == 1 || @_ == 2 || croak(q/Usage: $handle->can_read([timeout])/);
    my $self = shift;
    return $self->_do_timeout('read', @_)
}

sub can_write {
    @_ == 1 || @_ == 2 || croak(q/Usage: $handle->can_write([timeout])/);
    my $self = shift;
    return $self->_do_timeout('write', @_)
}

my $prog = <<'EOP';
BEGIN {
   if ( defined &IO::Socket::SSL::CAN_IPV6 ) {
      *CAN_IPV6 = \*IO::Socket::SSL::CAN_IPV6;
   }
   else {
      constant->import( CAN_IPV6 => '' );
   }
   my %const = (
      NID_CommonName => 13,
      GEN_DNS => 2,
      GEN_IPADD => 7,
   );
   while ( my ($name,$value) = each %const ) {
      no strict 'refs';
      *{$name} = UNIVERSAL::can( 'Net::SSLeay', $name ) || sub { $value };
   }
}
{
   my %dispatcher = (
      issuer =>  sub { Net::SSLeay::X509_NAME_oneline( Net::SSLeay::X509_get_issuer_name( shift )) },
      subject => sub { Net::SSLeay::X509_NAME_oneline( Net::SSLeay::X509_get_subject_name( shift )) },
   );
   if ( $Net::SSLeay::VERSION >= 1.30 ) {
      $dispatcher{commonName} = sub {
         my $cn = Net::SSLeay::X509_NAME_get_text_by_NID(
            Net::SSLeay::X509_get_subject_name( shift ), NID_CommonName);
         $cn =~s{\0$}{}; # work around Bug in Net::SSLeay <1.33
         $cn;
      }
   } else {
      $dispatcher{commonName} = sub {
         croak "you need at least Net::SSLeay version 1.30 for getting commonName"
      }
   }

   if ( $Net::SSLeay::VERSION >= 1.33 ) {
      $dispatcher{subjectAltNames} = sub { Net::SSLeay::X509_get_subjectAltNames( shift ) };
   } else {
      $dispatcher{subjectAltNames} = sub {
         return;
      };
   }

   $dispatcher{authority} = $dispatcher{issuer};
   $dispatcher{owner}     = $dispatcher{subject};
   $dispatcher{cn}        = $dispatcher{commonName};

   sub _peer_certificate {
      my ($self, $field) = @_;
      my $ssl = $self->_get_ssl_object or return;

      my $cert = ${*$self}{_SSL_certificate}
         ||= Net::SSLeay::get_peer_certificate($ssl)
         or return $self->error("Could not retrieve peer certificate");

      if ($field) {
         my $sub = $dispatcher{$field} or croak
            "invalid argument for peer_certificate, valid are: ".join( " ",keys %dispatcher ).
            "\nMaybe you need to upgrade your Net::SSLeay";
         return $sub->($cert);
      } else {
         return $cert
      }
   }


   my %scheme = (
      ldap => {
         wildcards_in_cn    => 0,
         wildcards_in_alt => 'leftmost',
         check_cn         => 'always',
      },
      http => {
         wildcards_in_cn    => 'anywhere',
         wildcards_in_alt => 'anywhere',
         check_cn         => 'when_only',
      },
      smtp => {
         wildcards_in_cn    => 0,
         wildcards_in_alt => 0,
         check_cn         => 'always'
      },
      none => {}, # do not check
   );

   $scheme{www}  = $scheme{http}; # alias
   $scheme{xmpp} = $scheme{http}; # rfc 3920
   $scheme{pop3} = $scheme{ldap}; # rfc 2595
   $scheme{imap} = $scheme{ldap}; # rfc 2595
   $scheme{acap} = $scheme{ldap}; # rfc 2595
   $scheme{nntp} = $scheme{ldap}; # rfc 4642
   $scheme{ftp}  = $scheme{http}; # rfc 4217


   sub _verify_hostname_of_cert {
      my $identity = shift;
      my $cert = shift;
      my $scheme = shift || 'none';
      if ( ! ref($scheme) ) {
         $scheme = $scheme{$scheme} or croak "scheme $scheme not defined";
      }

      return 1 if ! %$scheme; # 'none'

      my $commonName = $dispatcher{cn}->($cert);
      my @altNames   = $dispatcher{subjectAltNames}->($cert);

      if ( my $sub = $scheme->{callback} ) {
         return $sub->($identity,$commonName,@altNames);
      }


      my $ipn;
      if ( CAN_IPV6 and $identity =~m{:} ) {
         $ipn = IO::Socket::SSL::inet_pton(IO::Socket::SSL::AF_INET6,$identity)
            or croak "'$identity' is not IPv6, but neither IPv4 nor hostname";
      } elsif ( $identity =~m{^\d+\.\d+\.\d+\.\d+$} ) {
         $ipn = IO::Socket::SSL::inet_aton( $identity ) or croak "'$identity' is not IPv4, but neither IPv6 nor hostname";
      } else {
         if ( $identity =~m{[^a-zA-Z0-9_.\-]} ) {
            $identity =~m{\0} and croak("name '$identity' has \\0 byte");
            $identity = IO::Socket::SSL::idn_to_ascii($identity) or
               croak "Warning: Given name '$identity' could not be converted to IDNA!";
         }
      }

      my $check_name = sub {
         my ($name,$identity,$wtyp) = @_;
         $wtyp ||= '';
         my $pattern;
         if ( $wtyp eq 'anywhere' and $name =~m{^([a-zA-Z0-9_\-]*)\*(.+)} ) {
            $pattern = qr{^\Q$1\E[a-zA-Z0-9_\-]*\Q$2\E$}i;
         } elsif ( $wtyp eq 'leftmost' and $name =~m{^\*(\..+)$} ) {
            $pattern = qr{^[a-zA-Z0-9_\-]*\Q$1\E$}i;
         } else {
            $pattern = qr{^\Q$name\E$}i;
         }
         return $identity =~ $pattern;
      };

      my $alt_dnsNames = 0;
      while (@altNames) {
         my ($type, $name) = splice (@altNames, 0, 2);
         if ( $ipn and $type == GEN_IPADD ) {
            return 1 if $ipn eq $name;

         } elsif ( ! $ipn and $type == GEN_DNS ) {
            $name =~s/\s+$//; $name =~s/^\s+//;
            $alt_dnsNames++;
            $check_name->($name,$identity,$scheme->{wildcards_in_alt})
               and return 1;
         }
      }

      if ( ! $ipn and (
         $scheme->{check_cn} eq 'always' or
         $scheme->{check_cn} eq 'when_only' and !$alt_dnsNames)) {
         $check_name->($commonName,$identity,$scheme->{wildcards_in_cn})
            and return 1;
      }

      return 0; # no match
   }
}
EOP

eval { require IO::Socket::SSL };
if ( $INC{"IO/Socket/SSL.pm"} ) {
   eval $prog;
   die $@ if $@;
}

1;
}
# ###########################################################################
# End HTTPMicro package
# ###########################################################################

# ###########################################################################
# VersionCheck package
# This package is a copy without comments from the original.  The original
# with comments and its test file can be found in the Bazaar repository at,
#   lib/VersionCheck.pm
#   t/lib/VersionCheck.t
# See https://launchpad.net/percona-toolkit for more information.
# ###########################################################################
{
package VersionCheck;


use strict;
use warnings FATAL => 'all';
use English qw(-no_match_vars);

use constant PTDEBUG => $ENV{PTDEBUG} || 0;

use Data::Dumper;
local $Data::Dumper::Indent    = 1;
local $Data::Dumper::Sortkeys  = 1;
local $Data::Dumper::Quotekeys = 0;

use Digest::MD5 qw(md5_hex);
use Sys::Hostname qw(hostname);
use File::Basename qw();
use File::Spec;
use FindBin qw();

eval {
   require Percona::Toolkit;
   require HTTPMicro;
};

{
   my $file    = 'percona-version-check';
   my $home    = $ENV{HOME} || $ENV{HOMEPATH} || $ENV{USERPROFILE} || '.';
   my @vc_dirs = (
      '/etc/percona',
      '/etc/percona-toolkit',
      '/tmp',
      "$home",
   );

   if ($ENV{PTDEBUG_VERSION_CHECK_HOME}) {
       @vc_dirs = ( $ENV{PTDEBUG_VERSION_CHECK_HOME} );
   }

   sub version_check_file {
      foreach my $dir ( @vc_dirs ) {
         if ( -d $dir && -w $dir ) {
            PTDEBUG && _d('Version check file', $file, 'in', $dir);
            return $dir . '/' . $file;
         }
      }
      PTDEBUG && _d('Version check file', $file, 'in', $ENV{PWD});
      return $file;  # in the CWD
   } 
}

sub version_check_time_limit {
   return 60 * 60 * 24;  # one day
}


sub version_check {
   my (%args) = @_;

   my $instances = $args{instances} || [];
   my $instances_to_check;

   PTDEBUG && _d('FindBin::Bin:', $FindBin::Bin);
   if ( !$args{force} ) {
      if ( $FindBin::Bin
           && (-d "$FindBin::Bin/../.bzr" || -d "$FindBin::Bin/../../.bzr") ) {
         PTDEBUG && _d("$FindBin::Bin/../.bzr disables --version-check");
         return;
      }
   }

   eval {
      foreach my $instance ( @$instances ) {
         my ($name, $id) = get_instance_id($instance);
         $instance->{name} = $name;
         $instance->{id}   = $id;
      }

      push @$instances, { name => 'system', id => 0 };

      $instances_to_check = get_instances_to_check(
         instances => $instances,
         vc_file   => $args{vc_file},  # testing
         now       => $args{now},      # testing
      );
      PTDEBUG && _d(scalar @$instances_to_check, 'instances to check');
      return unless @$instances_to_check;

      my $protocol = 'https';  # optimistic, but...
      eval { require IO::Socket::SSL; };
      if ( $EVAL_ERROR ) {
         PTDEBUG && _d($EVAL_ERROR);
         $protocol = 'http';
      }
      PTDEBUG && _d('Using', $protocol);

      my $advice = pingback(
         instances => $instances_to_check,
         protocol  => $protocol,
         url       => $args{url}                       # testing
                   || $ENV{PERCONA_VERSION_CHECK_URL}  # testing
                   || "$protocol://v.percona.com",
      );
      if ( $advice ) {
         PTDEBUG && _d('Advice:', Dumper($advice));
         if ( scalar @$advice > 1) {
            print "\n# " . scalar @$advice . " software updates are "
               . "available:\n";
         }
         else {
            print "\n# A software update is available:\n";
         }
         print join("\n", map { "#   * $_" } @$advice), "\n\n";
      }
   };
   if ( $EVAL_ERROR ) {
      PTDEBUG && _d('Version check failed:', $EVAL_ERROR);
   }

   if ( @$instances_to_check ) {
      eval {
         update_check_times(
            instances => $instances_to_check,
            vc_file   => $args{vc_file},  # testing
            now       => $args{now},      # testing
         );
      };
      if ( $EVAL_ERROR ) {
         PTDEBUG && _d('Error updating version check file:', $EVAL_ERROR);
      }
   }

   if ( $ENV{PTDEBUG_VERSION_CHECK} ) {
      warn "Exiting because the PTDEBUG_VERSION_CHECK "
         . "environment variable is defined.\n";
      exit 255;
   }

   return;
}

sub get_instances_to_check {
   my (%args) = @_;

   my $instances = $args{instances};
   my $now       = $args{now}     || int(time);
   my $vc_file   = $args{vc_file} || version_check_file();

   if ( !-f $vc_file ) {
      PTDEBUG && _d('Version check file', $vc_file, 'does not exist;',
         'version checking all instances');
      return $instances;
   }

   open my $fh, '<', $vc_file or die "Cannot open $vc_file: $OS_ERROR";
   chomp(my $file_contents = do { local $/ = undef; <$fh> });
   PTDEBUG && _d('Version check file', $vc_file, 'contents:', $file_contents);
   close $fh;
   my %last_check_time_for = $file_contents =~ /^([^,]+),(.+)$/mg;

   my $check_time_limit = version_check_time_limit();
   my @instances_to_check;
   foreach my $instance ( @$instances ) {
      my $last_check_time = $last_check_time_for{ $instance->{id} };
      PTDEBUG && _d('Intsance', $instance->{id}, 'last checked',
         $last_check_time, 'now', $now, 'diff', $now - ($last_check_time || 0),
         'hours until next check',
         sprintf '%.2f',
            ($check_time_limit - ($now - ($last_check_time || 0))) / 3600);
      if ( !defined $last_check_time
           || ($now - $last_check_time) >= $check_time_limit ) {
         PTDEBUG && _d('Time to check', Dumper($instance));
         push @instances_to_check, $instance;
      }
   }

   return \@instances_to_check;
}

sub update_check_times {
   my (%args) = @_;

   my $instances = $args{instances};
   my $now       = $args{now}     || int(time);
   my $vc_file   = $args{vc_file} || version_check_file();
   PTDEBUG && _d('Updating last check time:', $now);

   my %all_instances = map {
      $_->{id} => { name => $_->{name}, ts => $now }
   } @$instances;

   if ( -f $vc_file ) {
      open my $fh, '<', $vc_file or die "Cannot read $vc_file: $OS_ERROR";
      my $contents = do { local $/ = undef; <$fh> };
      close $fh;

      foreach my $line ( split("\n", ($contents || '')) ) {
         my ($id, $ts) = split(',', $line);
         if ( !exists $all_instances{$id} ) {
            $all_instances{$id} = { ts => $ts };  # original ts, not updated
         }
      }
   }

   open my $fh, '>', $vc_file or die "Cannot write to $vc_file: $OS_ERROR";
   foreach my $id ( sort keys %all_instances ) {
      PTDEBUG && _d('Updated:', $id, Dumper($all_instances{$id}));
      print { $fh } $id . ',' . $all_instances{$id}->{ts} . "\n";
   }
   close $fh;

   return;
}

sub get_instance_id {
   my ($instance) = @_;

   my $dbh = $instance->{dbh};
   my $dsn = $instance->{dsn};

   my $sql = q{SELECT CONCAT(@@hostname, @@port)};
   PTDEBUG && _d($sql);
   my ($name) = eval { $dbh->selectrow_array($sql) };
   if ( $EVAL_ERROR ) {
      PTDEBUG && _d($EVAL_ERROR);
      $sql = q{SELECT @@hostname};
      PTDEBUG && _d($sql);
      ($name) = eval { $dbh->selectrow_array($sql) };
      if ( $EVAL_ERROR ) {
         PTDEBUG && _d($EVAL_ERROR);
         $name = ($dsn->{h} || 'localhost') . ($dsn->{P} || 3306);
      }
      else {
         $sql = q{SHOW VARIABLES LIKE 'port'};
         PTDEBUG && _d($sql);
         my (undef, $port) = eval { $dbh->selectrow_array($sql) };
         PTDEBUG && _d('port:', $port);
         $name .= $port || '';
      }
   }
   my $id = md5_hex($name);

   PTDEBUG && _d('MySQL instance:', $id, $name, Dumper($dsn));

   return $name, $id;
}


sub pingback {
   my (%args) = @_;
   my @required_args = qw(url instances);
   foreach my $arg ( @required_args ) {
      die "I need a $arg arugment" unless $args{$arg};
   }
   my $url       = $args{url};
   my $instances = $args{instances};

   my $ua = $args{ua} || HTTPMicro->new( timeout => 3 );

   my $response = $ua->request('GET', $url);
   PTDEBUG && _d('Server response:', Dumper($response));
   die "No response from GET $url"
      if !$response;
   die("GET on $url returned HTTP status $response->{status}; expected 200\n",
       ($response->{content} || '')) if $response->{status} != 200;
   die("GET on $url did not return any programs to check")
      if !$response->{content};

   my $items = parse_server_response(
      response => $response->{content}
   );
   die "Failed to parse server requested programs: $response->{content}"
      if !scalar keys %$items;
      
   my $versions = get_versions(
      items     => $items,
      instances => $instances,
   );
   die "Failed to get any program versions; should have at least gotten Perl"
      if !scalar keys %$versions;

   my $client_content = encode_client_response(
      items      => $items,
      versions   => $versions,
      general_id => md5_hex( hostname() ),
   );

   my $client_response = {
      headers => { "X-Percona-Toolkit-Tool" => File::Basename::basename($0) },
      content => $client_content,
   };
   PTDEBUG && _d('Client response:', Dumper($client_response));

   $response = $ua->request('POST', $url, $client_response);
   PTDEBUG && _d('Server suggestions:', Dumper($response));
   die "No response from POST $url $client_response"
      if !$response;
   die "POST $url returned HTTP status $response->{status}; expected 200"
      if $response->{status} != 200;

   return unless $response->{content};

   $items = parse_server_response(
      response   => $response->{content},
      split_vars => 0,
   );
   die "Failed to parse server suggestions: $response->{content}"
      if !scalar keys %$items;
   my @suggestions = map { $_->{vars} }
                     sort { $a->{item} cmp $b->{item} }
                     values %$items;

   return \@suggestions;
}

sub encode_client_response {
   my (%args) = @_;
   my @required_args = qw(items versions general_id);
   foreach my $arg ( @required_args ) {
      die "I need a $arg arugment" unless $args{$arg};
   }
   my ($items, $versions, $general_id) = @args{@required_args};

   my @lines;
   foreach my $item ( sort keys %$items ) {
      next unless exists $versions->{$item};
      if ( ref($versions->{$item}) eq 'HASH' ) {
         my $mysql_versions = $versions->{$item};
         for my $id ( sort keys %$mysql_versions ) {
            push @lines, join(';', $id, $item, $mysql_versions->{$id});
         }
      }
      else {
         push @lines, join(';', $general_id, $item, $versions->{$item});
      }
   }

   my $client_response = join("\n", @lines) . "\n";
   return $client_response;
}

sub parse_server_response {
   my (%args) = @_;
   my @required_args = qw(response);
   foreach my $arg ( @required_args ) {
      die "I need a $arg arugment" unless $args{$arg};
   }
   my ($response) = @args{@required_args};

   my %items = map {
      my ($item, $type, $vars) = split(";", $_);
      if ( !defined $args{split_vars} || $args{split_vars} ) {
         $vars = [ split(",", ($vars || '')) ];
      }
      $item => {
         item => $item,
         type => $type,
         vars => $vars,
      };
   } split("\n", $response);

   PTDEBUG && _d('Items:', Dumper(\%items));

   return \%items;
}

my %sub_for_type = (
   os_version          => \&get_os_version,
   perl_version        => \&get_perl_version,
   perl_module_version => \&get_perl_module_version,
   mysql_variable      => \&get_mysql_variable,
);

sub valid_item {
   my ($item) = @_;
   return unless $item;
   if ( !exists $sub_for_type{ $item->{type} } ) {
      PTDEBUG && _d('Invalid type:', $item->{type});
      return 0;
   }
   return 1;
}

sub get_versions {
   my (%args) = @_;
   my @required_args = qw(items);
   foreach my $arg ( @required_args ) {
      die "I need a $arg arugment" unless $args{$arg};
   }
   my ($items) = @args{@required_args};

   my %versions;
   foreach my $item ( values %$items ) {
      next unless valid_item($item);
      eval {
         my $version = $sub_for_type{ $item->{type} }->(
            item      => $item,
            instances => $args{instances},
         );
         if ( $version ) {
            chomp $version unless ref($version);
            $versions{$item->{item}} = $version;
         }
      };
      if ( $EVAL_ERROR ) {
         PTDEBUG && _d('Error getting version for', Dumper($item), $EVAL_ERROR);
      }
   }

   return \%versions;
}


sub get_os_version {
   if ( $OSNAME eq 'MSWin32' ) {
      require Win32;
      return Win32::GetOSDisplayName();
   }

  chomp(my $platform = `uname -s`);
  PTDEBUG && _d('platform:', $platform);
  return $OSNAME unless $platform;

   chomp(my $lsb_release
            = `which lsb_release 2>/dev/null | awk '{print \$1}'` || '');
   PTDEBUG && _d('lsb_release:', $lsb_release);

   my $release = "";

   if ( $platform eq 'Linux' ) {
      if ( -f "/etc/fedora-release" ) {
         $release = `cat /etc/fedora-release`;
      }
      elsif ( -f "/etc/redhat-release" ) {
         $release = `cat /etc/redhat-release`;
      }
      elsif ( -f "/etc/system-release" ) {
         $release = `cat /etc/system-release`;
      }
      elsif ( $lsb_release ) {
         $release = `$lsb_release -ds`;
      }
      elsif ( -f "/etc/lsb-release" ) {
         $release = `grep DISTRIB_DESCRIPTION /etc/lsb-release`;
         $release =~ s/^\w+="([^"]+)".+/$1/;
      }
      elsif ( -f "/etc/debian_version" ) {
         chomp(my $rel = `cat /etc/debian_version`);
         $release = "Debian $rel";
         if ( -f "/etc/apt/sources.list" ) {
             chomp(my $code_name = `awk '/^deb/ {print \$3}' /etc/apt/sources.list | awk -F/ '{print \$1}'| awk 'BEGIN {FS="|"} {print \$1}' | sort | uniq -c | sort -rn | head -n1 | awk '{print \$2}'`);
             $release .= " ($code_name)" if $code_name;
         }
      }
      elsif ( -f "/etc/os-release" ) { # openSUSE
         chomp($release = `grep PRETTY_NAME /etc/os-release`);
         $release =~ s/^PRETTY_NAME="(.+)"$/$1/;
      }
      elsif ( `ls /etc/*release 2>/dev/null` ) {
         if ( `grep DISTRIB_DESCRIPTION /etc/*release 2>/dev/null` ) {
            $release = `grep DISTRIB_DESCRIPTION /etc/*release | head -n1`;
         }
         else {
            $release = `cat /etc/*release | head -n1`;
         }
      }
   }
   elsif ( $platform =~ m/(?:BSD|^Darwin)$/ ) {
      my $rel = `uname -r`;
      $release = "$platform $rel";
   }
   elsif ( $platform eq "SunOS" ) {
      my $rel = `head -n1 /etc/release` || `uname -r`;
      $release = "$platform $rel";
   }

   if ( !$release ) {
      PTDEBUG && _d('Failed to get the release, using platform');
      $release = $platform;
   }
   chomp($release);

   $release =~ s/^"|"$//g;

   PTDEBUG && _d('OS version =', $release);
   return $release;
}

sub get_perl_version {
   my (%args) = @_;
   my $item = $args{item};
   return unless $item;

   my $version = sprintf '%vd', $PERL_VERSION;
   PTDEBUG && _d('Perl version', $version);
   return $version;
}

sub get_perl_module_version {
   my (%args) = @_;
   my $item = $args{item};
   return unless $item;

   my $var     = '$' . $item->{item} . '::VERSION';
   my $version = eval "use $item->{item}; $var;";
   PTDEBUG && _d('Perl version for', $var, '=', $version);
   return $version;
}

sub get_mysql_variable {
   return get_from_mysql(
      show => 'VARIABLES',
      @_,
   );
}

sub get_from_mysql {
   my (%args) = @_;
   my $show      = $args{show};
   my $item      = $args{item};
   my $instances = $args{instances};
   return unless $show && $item;

   if ( !$instances || !@$instances ) {
      PTDEBUG && _d('Cannot check', $item,
         'because there are no MySQL instances');
      return;
   }

   my @versions;
   my %version_for;
   foreach my $instance ( @$instances ) {
      next unless $instance->{id};  # special system instance has id=0
      my $dbh = $instance->{dbh};
      local $dbh->{FetchHashKeyName} = 'NAME_lc';
      my $sql = qq/SHOW $show/;
      PTDEBUG && _d($sql);
      my $rows = $dbh->selectall_hashref($sql, 'variable_name');

      my @versions;
      foreach my $var ( @{$item->{vars}} ) {
         $var = lc($var);
         my $version = $rows->{$var}->{value};
         PTDEBUG && _d('MySQL version for', $item->{item}, '=', $version,
            'on', $instance->{name});
         push @versions, $version;
      }
      $version_for{ $instance->{id} } = join(' ', @versions);
   }

   return \%version_for;
}

sub _d {
   my ($package, undef, $line) = caller 0;
   @_ = map { (my $temp = $_) =~ s/\n/\n# /g; $temp; }
        map { defined $_ ? $_ : 'undef' }
        @_;
   print STDERR "# $package:$line $PID ", join(' ', @_), "\n";
}

1;
}
# ###########################################################################
# End VersionCheck package
# ###########################################################################

######################################################################
# program execution begins here
######################################################################

# check command-line args
check_args();

# print program version and copyright
print_version();

# initialize global variables and perform some checks
if ($option_backup) {
    # backup
    %mysql = mysql_connect(abort_on_error => 1);

    check_server_version();

    if ($option_version_check) {
        $now = current_time();
        print STDERR
            "$now  $prefix Executing a version check against the server...\n";

        # Redirect STDOUT to STDERR, as VersionCheck prints alerts to STDOUT
        select STDERR;

        VersionCheck::version_check(
                                    force => 1,
                                    instances => [ {
                                                    dbh => $mysql{dbh},
                                                    dsn => $mysql{dsn}
                                                   }
                                                 ]
                                    );
        # Restore STDOUT as the default filehandle
        select STDOUT;

        $now = current_time();
        print STDERR "$now  $prefix Done.\n";
    }
}
init();

my $ibbackup_exit_code = 0;

if ($option_decompress || $option_decrypt) {
    # decrypt/decompress files
    if ($option_decompress && $option_decrypt) {
      # Call mode 0 first as it is fastest but may cause stream buffer overruns
      # requiring user invoke innobackupex twice, once for decrypt and again
      # for decompress. Mode 0 will take .qb.xbcrypt and run it through
      # xbcrypt | qpress > result. This should get all InnoDB files but any
      # metadata or MyISAM, etc. will not be .qp.xbcrypt, only .xbcrypt
      decrypt_decompress(0);
      # Now call mode 1 to decrypt any uncompressed files, those with only a
      # .xbcrypt extension
      decrypt_decompress(1);
      # Now, just in case there were any compressed files only, run throuh and
      # decompress any .qp files
      decrypt_decompress(2);
    } elsif ($option_decompress) {
      decrypt_decompress(2);
    } else {
      decrypt_decompress(1);
    }
} elsif ($option_copy_back) {
    # copy files from backup directory back to their original locations
    copy_back(0);
} elsif ($option_move_back) {
    # move files from backup directory back to their original locations
    copy_back(1);
} elsif ($option_apply_log) {
    # expand data files in backup directory by applying log files to them
    apply_log();
} else {
    # make a backup of InnoDB and MyISAM tables, indexes and .frm files.
    $ibbackup_exit_code = backup();

    mysql_close(\%mysql);
}

$now = current_time();

if ($option_stream eq 'tar') {
   print STDERR "$prefix You must use -i (--ignore-zeros) option for extraction of the tar stream.\n";
}

if ( $ibbackup_exit_code == 0 ) {
   # program has completed successfully
   print STDERR "$now  $prefix completed OK!\n";
}
else {
   print STDERR "$now  $prefix $option_ibbackup_binary failed! (exit code $ibbackup_exit_code)  The backup may not be complete.\n";
}

exit $ibbackup_exit_code;

######################################################################
# end of program execution
######################################################################


#
# print_version subroutine prints program version and copyright.
#
sub print_version {
    my $distribution = "@XB_DISTRIBUTION@";

    printf(STDERR $copyright_notice);

    if ($distribution) {
        printf STDERR "Get the latest version of Percona XtraBackup, " .
            "documentation, and help resources:\n";
        printf STDERR "http://www.percona.com/xb/$distribution\n\n";
    }
}


#
# usage subroutine prints instructions of how to use this program to stdout.
#
sub usage {
   my $msg = shift || '';
   pod2usage({ -msg => $msg, -verbose => 1, -exitval => "NOEXIT"});
   return 0;
}


#
# return current local time as string in form "070816 12:23:15"
#
sub current_time {
    return strftime("%y%m%d %H:%M:%S", localtime());
}

#
# Global initialization:
#
# - embed the HTTPMicro and VersionCheck modules by setting %INC to this
#   file, so Perl does not try to load those modules from @INC
#
# - Override the 'die' builtin to customize the format of error messages
#
# - Setup signal handlers to terminate gracefully on fatal signals
#
BEGIN {
    $INC{$_} = __FILE__ for map { (my $pkg = "$_.pm") =~ s!::!/!g; $pkg } (qw(
      HTTPMicro
      VersionCheck
      ));

    *CORE::GLOBAL::die = sub {

        print STDERR longmess("$prefix got a fatal error with the following ".
                              "stacktrace:");

        print STDERR "$prefix Error: @_";

        # Mimic the default 'die' behavior: append information about the caller
        # and the new line, if the argument does not already end with a newline
        if ($_[-1] !~ m(\n$)) {
            my ($pkg, $file, $line) = caller 0;

            print STDERR " at $file line $line.\n";
        }

        exit(1);
    };

    foreach ('HUP', 'INT', 'QUIT', 'ABRT', 'PIPE', 'TERM', 'XFSZ') {
        $SIG{$_} = \&catch_fatal_signal;
    }
}

#
# Make sure all child processes are killed on exit. This doesn't cover cases
# when innobackupex is terminated with a signal.
#
END {
    # We only want to execute this in the parent script process, and ignore
    # for all child processes it may spawn
    if ($$ == $innobackupex_pid) {
        kill_child_processes();
    }
}

#
# kill child processes spawned by the script
#
sub kill_child_processes {
    if ($ibbackup_pid) {
        kill($kill_signal, $ibbackup_pid);
        wait_for_ibbackup_finish();
    }
    stop_query_killer();
}

sub catch_fatal_signal {
    # Don't print anything if a child process is terminated with a signal
    if ($$ == $innobackupex_pid) {
        die "Terminated with SIG$_[0]"
    } else {
        exit(1);
    }
}

#
# Finds all files that match the given pattern recursively beneath
# the given directory. Returns a list of files with their paths relative 
# to the starting search directory.
#
sub find_all_matching_files {
  my $dir = shift;
  my $pattern = shift;
  my $child;
  my @dirlist;
  my @retlist;
 
  opendir(FINDDIR, $dir)
        || die "Can't open directory '$dir': $!";

  while (defined($child = readdir(FINDDIR))) {
    next if $child eq "." || $child eq "..";
    if ($child =~ m/$pattern/) { push(@retlist, "$dir/$child"); }
    elsif ( -d "$dir/$child") { push(@dirlist, "$dir/$child"); }
  }
  closedir(FINDDIR);

  foreach $child (@dirlist) {
    push(@retlist, find_all_matching_files("$child", "$pattern"));
  }
  return (@retlist);
}

#
# decrypts and/or decompresses an individual file amd removes the source
# on success.
# mode is same mode as defined for sub decrypt_decompress()
sub decrypt_decompress_file {
  my $mode = shift;
  my $file = shift;
  my $file_ext = shift;
  my $decrypt_opts = shift;

  my $file_cmd;
  my $dest_file=substr($file, 0, length($file) - length($file_ext));

  if ($mode == 0) {
    $file_cmd = "xbcrypt --decrypt $decrypt_opts --input=" . $file . " | qpress -dio > " . $dest_file;
  } elsif ($mode == 1) {
    $file_cmd = "xbcrypt --decrypt $decrypt_opts --input=" . $file . " --output=" . $dest_file;
  } elsif ($mode == 2) {
    $file_cmd = "qpress -do " . $file . " > " . $dest_file;
  } else {
    die "Unknown decrypt_decompress mode : $mode";
  }
  print STDERR "$prefix $file_cmd\n";
  system("$file_cmd") && die "$file_cmd failed with $!";
  system("rm -f $file");
}

#
# Decrypts and decompresses a backup made with --compress and/or --encrypt
#
# mode 0 = decrypt and decompress .qp.xbcrypt files
# mode 1 = decrypt .xbcrypt files
# mode 2 = decompress .qp files
#
sub decrypt_decompress {
  my $mode = shift;
  my $file_ext;
  my $decrypt_opts = '';
  my @files;

  # first, build out the decryption opts. They will not be used and option
  # values may not be specified if doing decompress only.
  if ($mode != 2) {
    if ($option_decrypt) {
      $decrypt_opts = "--encrypt-algo=$option_decrypt"
    }
    if ($option_encrypt_key) {
      $decrypt_opts = $decrypt_opts . " --encrypt-key=$option_encrypt_key";
    } elsif ($option_encrypt_key_file) {
      $decrypt_opts = $decrypt_opts . " --encrypt-key-file=$option_encrypt_key_file";
    }
  }

  # based on the mode, determine which files we are interested in
  if ($mode == 0) {
    $file_ext = ".qp.xbcrypt";
  } elsif ($mode == 1) {
    $file_ext = ".xbcrypt";
  } elsif ($mode == 2) {
    $file_ext = ".qp";
  } else {
    die "Unknown decrypt_decompress mode : $mode";
  }

  # recursively find all files of interest in the backup set
  @files = find_all_matching_files("$backup_dir", "\\$file_ext\$");

  # runing serially
  if (!$option_parallel) {
    my $file;
    foreach $file (@files) {
      decrypt_decompress_file($mode, $file, $file_ext, $decrypt_opts);
    }
  # running parallel, FUN!
  } else {
    my @pids;
    my @workingfiles;
    my $file;
    my $freepidindex;
    
    # this will maintain the list of forks so we can wait for completion
    # and check return status as well as stay within the limits of the 
    # --parallel value
    $pids[$option_parallel - 1] = undef;

    # this is the file for which each fork is currently working on. just
    # used for logging/reporting purposed in case of a failure.
    $workingfiles[$option_parallel - 1] = undef;

    foreach $file (@files) {
      # this while loop below tries to find an unused child/fork slot, ie, one
      # that is not currently working/running. it will loop indefinitely,
      # iterating the number of slots and watching for a child to finish
      # and use that now vacant slot.
      $freepidindex=$option_parallel;
      while ($freepidindex >= $option_parallel) {
        for ($freepidindex = 0; $freepidindex < $option_parallel; $freepidindex++) {
          last if not defined $pids[$freepidindex];

          my $status = waitpid($pids[$freepidindex], WNOHANG);
          if ($status == $pids[$freepidindex]) {
            my $childretcode = $? >> 8;
            if ($childretcode != 0) {
              die "Child failed on $workingfiles[$freepidindex] : $childretcode";
            }
            last;
          } elsif ($status == -1) {
            die "waitpid failed on $workingfiles[$freepidindex] : $status";
          }
        }
        # couldn't find a free slot yet so sleep for 1/10 th of a second or so.
        # sleeping for a much longer (even a second) can make the whole process
        # take longer when there are a lot of smaller files to process as they
        # can be done very quickly. 1/10th doesn't seem to add any real overhead
        # to this controlling process when compared to letting it spin freely.
        if ($freepidindex >= $option_parallel) {
          usleep(100000);
        }
      }
      # so now we have a slot identified by freepidindex, set up the workingfiles,
      # fork and do the work.
      $workingfiles[$freepidindex] = $file;
      $pids[$freepidindex] = fork();
      if ($pids[$freepidindex] == 0) {
        decrypt_decompress_file($mode, $file, $file_ext, $decrypt_opts);
        exit 0;
      }
    }
    # we've started processing of all files that we know about, now just need
    # to wait for the last bunch to finish up and check for any errors before
    # returning.
    for ($freepidindex = 0; $freepidindex < $option_parallel; $freepidindex++) {
      next if not defined $pids[$freepidindex];
      my $status = waitpid($pids[$freepidindex], 0);
      my $childretcode = $? >> 8;
      if ($status == $pids[$freepidindex]) {
        if ($childretcode != 0) {
          die "Child failed on $workingfiles[$freepidindex] : $childretcode";
        }
      } else {
        die "waitpid failed on $workingfiles[$freepidindex] : $status";
      }
    }
  }
}
 
#
# backup subroutine makes a backup of InnoDB and MyISAM tables, indexes and 
# .frm files. It connects to the database server and runs ibbackup as a child
# process.
#
sub backup {
    my $orig_datadir = get_option('datadir');
    my $suspend_file;
    my $buffer_pool_filename = get_option_safe('innodb_buffer_pool_filename',
                                               '');

    detect_mysql_capabilities_for_backup(\%mysql);


    # Do not allow --slave-info with a multi-threded non-GTID slave,
    # see https://bugs.launchpad.net/percona-xtrabackup/+bug/1372679
    if ($option_slave_info and $have_multi_threaded_slave
        and !$have_gtid_slave) {
        die "The --slave-info option requires GTID enabled for a " .
            "multi-threaded slave."
    }

    #
    # if one of the history incrementals is being used, try to grab the
    # innodb_to_lsn from the history table and set the option_incremental_lsn
    #
    if ($option_incremental && !$option_incremental_lsn) {
        if ($option_incremental_history_name) {
            my $query = "SELECT innodb_to_lsn ".
                        "FROM PERCONA_SCHEMA.xtrabackup_history ".
                        "WHERE name = '$option_incremental_history_name' ".
                        "AND innodb_to_lsn IS NOT NULL ".
                        "ORDER BY innodb_to_lsn DESC LIMIT 1";

            eval {
                $option_incremental_lsn =
                    $mysql{dbh}->selectrow_hashref($query)->{innodb_to_lsn};
            } || die("Error while attempting to find history record for name ".
                     "'$option_incremental_history_name'\n");
            print STDERR "$prefix --incremental-history-name=".
                "$option_incremental_history_name specified.".
                "Found and using lsn: $option_incremental_lsn";
        } elsif ($option_incremental_history_uuid) {
            my $query = "SELECT innodb_to_lsn ".
                        "FROM PERCONA_SCHEMA.xtrabackup_history ".
                        "WHERE uuid = '$option_incremental_history_uuid' ".
                        "AND innodb_to_lsn IS NOT NULL";

            eval {
                $option_incremental_lsn =
                    $mysql{dbh}->selectrow_hashref($query)->{innodb_to_lsn};
            } || die("Error while attempting to find history record for uuid ".
                     "'$option_incremental_history_uuid'\n");
            print STDERR "$prefix --incremental-history-uuid=".
                "$option_incremental_history_uuid specified.".
                "Found and using lsn: $option_incremental_lsn";
        }
    }

    # start ibbackup as a child process
    start_ibbackup();

    if ($option_incremental && $have_changed_page_bitmaps
        && !$option_incremental_force_scan) {
        $suspend_file = $work_dir . $xb_fn_suspended_at_start;
        wait_for_ibbackup_suspend($suspend_file);
        mysql_query(\%mysql, 'FLUSH NO_WRITE_TO_BINLOG CHANGED_PAGE_BITMAPS');
        resume_ibbackup($suspend_file);
    }

    # wait for ibbackup to suspend itself
    $suspend_file = $work_dir . $xb_fn_suspended_at_end;
    wait_for_ibbackup_suspend($suspend_file);

    if (!$option_no_lock) {
      if ($option_safe_slave_backup) {
        wait_for_safe_slave(\%mysql);
      }

      # make a prep copy before locking tables, if using rsync
      backup_files(1);

      # start counting how long lock is held for history
      $history_lock_time = time();

      # lock tables
      mysql_lock_tables(\%mysql);
    }

    # backup non-InnoDB files and tables
    # (or finalize the backup by syncing changes if using rsync)
    backup_files(0);

    # There is no need to stop slave thread before coping non-Innodb data when
    # --no-lock option is used because --no-lock option requires that no DDL or
    # DML to non-transaction tables can occur.
    if ($option_no_lock) {
        if ($option_safe_slave_backup) {
            wait_for_safe_slave(\%mysql);
        }
        if ($option_slave_info) {
            write_slave_info(\%mysql);
        }
    } else {
        mysql_lock_binlog(\%mysql);

        if ($option_slave_info) {
            write_slave_info(\%mysql);
        }
    }

    # The only reason why Galera/binlog info is written before
    # wait_for_ibbackup_log_copy_finish() is that after that call the xtrabackup
    # binary will start streamig a temporary copy of REDO log to stdout and
    # thus, any streaming from innobackupex would interfere. The only way to
    # avoid that is to have a single process, i.e. merge innobackupex and
    # xtrabackup.
    if ($option_galera_info) {
        write_galera_info(\%mysql);
        write_current_binlog_file(\%mysql);
    }

    write_binlog_info(\%mysql);

    if ($have_flush_engine_logs) {
        my $now = current_time();
        print STDERR "$now  $prefix Executing FLUSH " .
            "NO_WRITE_TO_BINLOG ENGINE LOGS...\n";
        mysql_query(\%mysql, "FLUSH NO_WRITE_TO_BINLOG ENGINE LOGS");
    }

    # resume XtraBackup and wait till it has finished
    resume_ibbackup($suspend_file);
    $suspend_file = $work_dir . $xb_fn_log_copied;
    wait_for_ibbackup_log_copy_finish($suspend_file);

    # release all locks
    if (!$option_no_lock) {
        mysql_unlock_all(\%mysql);
    }

    # calculate how long lock was held for history
    if ($option_no_lock) {
      $history_lock_time = 0;
    } else {
      $history_lock_time = time() - $history_lock_time;
    }

    if ( $option_safe_slave_backup && $sql_thread_started) {
      print STDERR "$prefix: Starting slave SQL thread\n";
      mysql_query(\%mysql, 'START SLAVE SQL_THREAD;');
    }

    my $ibbackup_exit_code = wait_for_ibbackup_finish();

    # copy ib_lru_dump
    # Copy buffer poll dump and/or LRU dump
    foreach my $dump_name ($buffer_pool_filename, 'ib_lru_dump') {
        if (!$option_rsync && $dump_name ne '' &&
            -e "$orig_datadir/$dump_name") {
        backup_file("$orig_datadir", "$dump_name", "$backup_dir/$dump_name")
      }
    }

    print STDERR "\n$prefix Backup created in directory '$backup_dir'\n";
    if ($mysql_binlog_position) {
        print STDERR "$prefix MySQL binlog position: $mysql_binlog_position\n";
    }
    if ($mysql_slave_position && $option_slave_info) {
        print STDERR "$prefix MySQL slave binlog position: $mysql_slave_position\n";
    }

    write_xtrabackup_info(\%mysql);

    return $ibbackup_exit_code;
}


#
# are_equal_innodb_data_file_paths subroutine checks if the given
# InnoDB data file option values are equal.
#   Parameters:
#     str1    InnoDB data file path option value
#     str2    InnoDB data file path option value
#   Return value:
#     1  if values are equal
#     0  otherwise
#
sub are_equal_innodb_data_file_paths {
    my $str1 = shift;
    my $str2 = shift;
    my @array1 = split(/;/, $str1);
    my @array2 = split(/;/, $str2);
    
    if ($#array1 != $#array2) { return 0; }

    for (my $i = 0; $i <= $#array1; $i++) {
        my @def1 = split(/:/, $array1[$i]);
        my @def2 = split(/:/, $array2[$i]);
        
        if ($#def1 != $#def2) { return 0; }

        for (my $j = 0; $j <= $#def1; $j++) {
            if ($def1[$j] ne $def2[$j]) { return 0; }
        }
    }
    return 1;
}        


#
# copy_if_exists subroutin attempt to copy a file from $src to $dst
# If the copy fails due to the file not existing, the error is ignored
#  Parameters:
#    $src    The source file to copy
#    $dst    The destination to copy to
#  Return value:
#    1  if copy was successful, or unsuccessful but the source file didn't exist
#    0  otherwise
#
sub copy_if_exists {
    my $src = shift;
    my $dst = shift;

    # Copy the file
    if( system("$CP_CMD \"$src\" \"$dst\"") != 0 ) {
        # if the copy failed, check if the file exists
        if( -e "$src" ) {
            # if the file exists, we had a real error
            return 0;
        }
    }
    # Success otherwise
    return 1;
}


#
# is_in_array subroutine checks if the given string is in the array.
#   Parameters:
#     str       a string
#     array_ref a reference to an array of strings
#   Return value:
#     1  if string is in the array
#     0  otherwise
# 
sub is_in_array {
    my $str = shift;
    my $array_ref = shift;

    if (grep { $str eq $_ } @{$array_ref}) {
        return 1;
    }
    return 0;
}

#
# Check if a given directory exists, or create one otherwise.
#

sub if_directory_exists {
    my $empty_dir = shift;
    my $is_directory_empty_comment = shift;
    if (! -d $empty_dir) {
        eval { mkpath($empty_dir) };
        if ($@) {
            die "Can not create $is_directory_empty_comment directory " .
                "'$empty_dir': $@";
        }
    }
}

#
# if_directory_exists_and_empty accepts two arguments:
# variable with directory name and comment.
# Sub checks that directory exists and is empty 
# or creates one if it doesn't exists.
# usage: is_directory_exists_and_empty($directory,"Comment");
#

sub if_directory_exists_and_empty {
    my $empty_dir = shift;
    my $is_directory_empty_comment = shift;

    if_directory_exists($empty_dir, $is_directory_empty_comment);

    if (!$option_force_non_empty_dirs) {
        opendir (my $dh, $empty_dir) or
            die "$is_directory_empty_comment directory '$empty_dir': Not a directory";
        if ( ! scalar( grep { $_ ne "." && $_ ne ".." && $_ ne "my.cnf" &&
                                  $_ ne "master.info"} readdir($dh)) == 0) {
            die "$is_directory_empty_comment directory '$empty_dir' is not empty!";
        }
        closedir($dh);
    }
}

#
# Fail with an error if file exists
#

sub die_if_exists {
    my $path = shift;

    if (-e $path) {
        die "Cannot overwrite file: $path";
    }
}

#
# copy() wrapper with error handling
#
sub copy_file {
    my $src_path = shift;
    my $dst_path = shift;

    print STDERR "$prefix Copying '$src_path' to '$dst_path'\n";
    copy($src_path, $dst_path) or die "copy failed: $!";
}

#
# move() wrapper with error handling
#
sub move_file {
    my $src_path = shift;
    my $dst_path = shift;

    print STDERR "$prefix Moving '$src_path' to '$dst_path'\n";
    move($src_path, $dst_path) or die "move failed: $!";
}


#
# Auxiliary function called from find() callbacks to copy or move files and create directories
# when necessary.
#
# SYNOPSIS
#
#     process_file(\&process_func);
#
#       &process_func is a code reference that does the actual file operation
#
sub process_file {
    my $process_func = shift;
    my $dst_path;

    if (/$process_dir_exclude_regexp/) {
	return;
    }

    # If requested, check if there is a "link" in the .isl file for an InnoDB
    # tablespace and use it as a destination path.
    if ($copy_dir_resolve_isl && $File::Find::name =~ m/\.ibd$/) {
        my $isl_path;

        ($isl_path = $File::Find::name) =~ s/\.ibd$/.isl/;
        if (-e "$isl_path") {
            my @lines;
            print STDERR "Found an .isl file for $File::Find::name\n";
            file_to_array($isl_path, \@lines);
            $dst_path = $lines[0];
            print STDERR "Using $dst_path as the destination path\n";

            my $dst_dir = File::Basename::dirname($dst_path);
            if (! -d $dst_dir) {
                print STDERR "Recursively creating directory $dst_dir\n";

                eval { mkpath($dst_dir) };
                if ($@) {
                    die "Could not create directory $dst_dir: $@";
                }
            }
        }
    }

    if (! defined $dst_path) {
        ($dst_path = $File::Find::name) =~ s/^$copy_dir_src/$copy_dir_dst/;
    }

    if (-d "$File::Find::name") {
	# Create the directory in the destination if necessary
	if (! -e "$dst_path") {
	    print STDERR "$prefix Creating directory '$dst_path'\n";
	    mkdir "$dst_path" or die "mkdir failed: $!";
	} elsif (! -d "$dst_path") {
	    die "$dst_path exists, but is not a directory";
	}
    } else {
	# Don't overwrite files unless $copy_dir_overwrite is 1
	if (!$copy_dir_overwrite && -e "$copy_dir_dst/$_") {
	    die "Failed to process file $File::Find::name: " .
		"not overwriting file $copy_dir_dst/$_";
	}

	&$process_func($File::Find::name, $dst_path);
        (my $rel_path = $File::Find::name) =~ s/^$copy_dir_src//;
        $processed_files{$rel_path} = 1;
    }
}

#
# find() callback to copy files
#
sub copy_file_callback {
    process_file(\&copy_file);
}

#
# find() callback to move files
#
sub move_file_callback {
    process_file(\&move_file);
}

#
# find() callback to remove files
#
sub remove_file_callback {
    if (/$process_dir_exclude_regexp/) {
        return;
    }
    if (-d "$File::Find::name") {
        return;
    }
    (my $rel_path = $File::Find::name) =~ s/^$cleanup_dir_dst//;
    if (!($rel_path =~ m/\//)) {
        return;
    }
    if (! exists $processed_files{$rel_path}) {
        unlink($File::Find::name) or
            die "Cannot remove file $File::Find::name";
    }
}

# copy_dir_recursively subroutine does a recursive copy of a specified directory
# excluding files matching a specifies regexp. If $overwrite is 1, it overwrites
# the existing files. If $resolve_isl is 1, it also resolves InnoDB .isl files,
# i.e. copies to the absolute path specified in the corresponding .isl file
# rather than the default location
#
# SYNOPSIS 
#
#     copy_dir_recursively($src_dir, $dst_dir, $exclude_regexp,
#                          $overwrite, $resolve_isl);
#
# TODO
#
#     use rsync when --rsync is specified
#
sub copy_dir_recursively {
    # Clean paths and remove trailing slashes if any
    $copy_dir_src = File::Spec->canonpath(shift);
    $copy_dir_dst = File::Spec->canonpath(shift);
    $process_dir_exclude_regexp = shift;
    $copy_dir_overwrite = shift;
    $copy_dir_resolve_isl = shift;

    undef %processed_files;
    find(\&copy_file_callback, $copy_dir_src);
}

#
# Similar to copy_dir_recursively, but moves files instead.
#
# SYNOPSIS
#
#     move_dir_recursively($src_dir, $dst_dir, $exclude_regexp,
#                          $overwrite, $resolve_isl);
#
# TODO
#
#     use rsync when --rsync is specified
#
sub move_dir_recursively {
    # Clean paths and remove trailing slashes if any
    $copy_dir_src = File::Spec->canonpath(shift);
    $copy_dir_dst = File::Spec->canonpath(shift);
    $process_dir_exclude_regexp = shift;
    $copy_dir_overwrite = shift;
    $copy_dir_resolve_isl = shift;

    undef %processed_files;
    find(\&move_file_callback, $copy_dir_src);
}

#
# cleanup_dir_recursively subroutine removes files from directory
# excluding files matching a specifies regexp and files listed in
# processed_files.
#
# SYNOPSIS 
#
#     cleanup_dir_recursively($dir, $exclude_regexp);
#
sub cleanup_dir_recursively {
    # Clean paths and remove trailing slashes if any
    $cleanup_dir_dst = File::Spec->canonpath(shift);
    $process_dir_exclude_regexp = shift;
    find(\&remove_file_callback, $cleanup_dir_dst);
}

#
# parse_innodb_data_file_path parse innodb_data_file_path and returns
# it components as array of hash refs. Each hash ref has two components
# the one named 'path' is the data file path as specified in the
# innodb-data-file-path, second one named 'filename' is the data file name
#
sub parse_innodb_data_file_path {
    my $innodb_data_file_path = shift;
    my @data_file_paths = ();

    foreach my $data_file (split(/;/, $innodb_data_file_path)) {
        my $data_file_path = (split(/:/,$data_file))[0];
        my $data_file_name = (split(/\/+/, $data_file_path))[-1];
        push(@data_file_paths, {path => $data_file_path,
                                filename => $data_file_name});
    }

    return @data_file_paths;
}

#
# copy_back subroutine copies data and index files from backup directory 
# back to their original locations.
#
sub copy_back {
    my $move_flag = shift;
    my $iblog_files = 'ib_logfile.*';
    my $ibundo_files = 'undo[0-9]{3}';
    my $excluded_files = 
        '\.\.?|backup-my\.cnf|xtrabackup_logfile|' .
        'xtrabackup_binary|xtrabackup_binlog_info|xtrabackup_checkpoints|' .
        '.*\.qp|' .
        '.*\.pmap|.*\.tmp|' .
        $iblog_files . '|'.
        $ibundo_files;
    my $file;

    my $orig_datadir = get_option('datadir');
    my $orig_ibdata_dir = get_option_safe('innodb_data_home_dir',
                                         $orig_datadir);
    my $orig_innodb_data_file_path = get_option_safe('innodb_data_file_path',
                                                     'ibdata1:10M:autoextend');
    my $orig_iblog_dir = get_option_safe('innodb_log_group_home_dir',
                                        $orig_datadir);
    my $orig_undo_dir = get_option_safe('innodb_undo_directory',
                                       $orig_datadir);

    if (has_option('innodb_doublewrite_file')) {
        my $doublewrite_file =
                get_option('innodb_doublewrite_file');
        $excluded_files = $excluded_files . '|' . $doublewrite_file;
    }

    # check whether files should be copied or moved to dest directory
    my $move_or_copy_file = $move_flag ? \&move_file : \&copy_file;
    my $move_or_copy_dir = $move_flag ?
        \&move_dir_recursively : \&copy_dir_recursively;
    my $operation = $move_flag ? "move" : "copy";


    # check that original data directories exist and they are empty
    if_directory_exists_and_empty($orig_datadir, "Original data");
    if ($orig_ibdata_dir) {
        if_directory_exists($orig_ibdata_dir, "Original InnoDB data");
    }
    if_directory_exists($orig_iblog_dir, "Original InnoDB log");
    if ($orig_undo_dir) {
        if_directory_exists($orig_undo_dir,
                                      "Original undo directory");
    }

    # make a list of all ibdata files in the backup directory and all
    # directories in the backup directory under which there are ibdata files
    foreach my $c (parse_innodb_data_file_path($orig_innodb_data_file_path)) {

        # check that the backup data file exists
        if (! -e "$backup_dir/$c->{filename}") {
            if (-e "$backup_dir/$c->{filename}.ibz") {
                die "Backup data file '$backup_dir/$c->{filename}' "
                  . "does not exist, but "
                  . "its compressed copy '$c->{path}.ibz' exists. Check "
                  . "that you have decompressed "
                  . " before attempting "
                  . "'$innobackup_script --copy-back ...'  "
                  . "or '$innobackup_script --move-back ...' !";
            } elsif (-e "$backup_dir/$c->{filename}.qp") {
                die "Backup data file '$backup_dir/$c->{filename}' "
                  . "does not exist, but "
                  . "its compressed copy '$c->{path}.qp' exists. Check "
                  . "that you have run "
                  . "'$innobackup_script --decompress "
                  . "...' before attempting "
                  . "'$innobackup_script --copy-back ...'  "
                  . "or '$innobackup_script --move-back ...' !";
            } elsif (-e "$backup_dir/$c->{filename}.xbcrypt") {
                die "Backup data file '$backup_dir/$c->{filename}' "
                  . "does not exist, but "
                  . "its compressed copy '$c->{path}.xbcrypt' exists. Check "
                  . "that you have run "
                  . "'$innobackup_script --decrypt "
                  . "...' before attempting "
                  . "'$innobackup_script --copy-back ...'  "
                  . "or '$innobackup_script --move-back ...' !";
            } else {
                die "Backup data file '$backup_dir/$c->{filename}' "
                    . "does not exist.";
            }
        }
	
        $excluded_files .= "|\Q$c->{filename}\E";
    }

    # copy files from backup dir to their original locations

    # copy files to original data directory
    my $excluded_regexp = '^(' . $excluded_files . ')$';
    print STDERR "$prefix Starting to $operation files in '$backup_dir'\n"; 
    print STDERR "$prefix back to original data directory '$orig_datadir'\n";
    &$move_or_copy_dir($backup_dir, $orig_datadir, $excluded_regexp, 0, 1);

    # copy InnoDB data files to original InnoDB data directory
    print STDERR "\n$prefix Starting to $operation InnoDB system tablespace\n";
    print STDERR "$prefix in '$backup_dir'\n";
    print STDERR "$prefix back to original InnoDB data directory '$orig_ibdata_dir'\n";
    foreach my $c (parse_innodb_data_file_path($orig_innodb_data_file_path)) {
        # get the relative pathname of a data file
        $src_name = escape_path("$backup_dir/$c->{filename}");
        if ($orig_ibdata_dir) {
            $dst_name = escape_path("$orig_ibdata_dir/$c->{path}");
        } else {
            # If innodb_data_home_dir is empty, but file path(s) in
            # innodb_data_file_path are relative, InnoDB treats them as if
            # innodb_data_home_dir was the same as datadir.

            my $dst_root = ($c->{path} =~ /^\//) ? "" : $orig_datadir;
            $dst_name = escape_path("$dst_root/$c->{path}");
        }
        die_if_exists($dst_name);
        &$move_or_copy_file($src_name, $dst_name);
    }

    # copy InnoDB undo tablespaces to innodb_undo_directory (if specified), or
    # to the InnoDB data directory
    print STDERR "\n$prefix Starting to $operation InnoDB undo tablespaces\n";
    print STDERR "$prefix in '$backup_dir'\n";
    print STDERR "$prefix back to '$orig_undo_dir'\n";
    opendir(DIR, $backup_dir)
        || die "Can't open directory '$backup_dir': $!";
    while (defined($file = readdir(DIR))) {
        if ($file =~ /^$ibundo_files$/ && -f "$backup_dir/$file") {
            $src_name = escape_path("$backup_dir/$file");
            $dst_name = escape_path("$orig_undo_dir/$file");
            die_if_exists($dst_name);
            &$move_or_copy_file($src_name, $dst_name);
        }
    }
    closedir(DIR);

    # copy InnoDB log files to original InnoDB log directory
    opendir(DIR, $backup_dir) 
        || die "Can't open directory '$backup_dir': $!";
    print STDERR "\n$prefix Starting to $operation InnoDB log files\n";
    print STDERR "$prefix in '$backup_dir'\n";
    print STDERR "$prefix back to original InnoDB log directory '$orig_iblog_dir'\n";
    while (defined($file = readdir(DIR))) {
        if ($file =~ /^$iblog_files$/ && -f "$backup_dir/$file") {
            $src_name = escape_path("$backup_dir/$file");
            $dst_name = escape_path("$orig_iblog_dir/$file");
            die_if_exists($dst_name);
            &$move_or_copy_file($src_name, $dst_name);
        }
    }
    closedir(DIR);

    print STDERR "$prefix Finished copying back files.\n\n";
}


#
# apply_log subroutine prepares a backup for starting database server
# on the backup. It applies InnoDB log files to the InnoDB data files.
#
sub apply_log {
    my $rcode;
    my $cmdline = '';
    my $cmdline_copy = '';
    my $options = '';

    $options = $options . " --defaults-file=\"${backup_dir}/backup-my.cnf\" ";

    if ($option_defaults_extra_file) {
        $options = $options . " --defaults-extra-file=\"$option_defaults_extra_file\" ";
    }

    if ($option_defaults_group) {
        $options = $options . " --defaults-group=\"$option_defaults_group\" ";
    }

    $options = $options . "--prepare --target-dir=$backup_dir";

    if ($option_export) {
        $options = $options . ' --export';
    }
    if ($option_redo_only) {
        $options = $options . ' --apply-log-only';
    }
    if ($option_use_memory) {
        $options = $options . " --use-memory=$option_use_memory";
    }

    if ($option_incremental_dir) {
        $options = $options . " --incremental-dir=$option_incremental_dir";
    }

    if ($option_tmpdir) {
        $options .= " --tmpdir=$option_tmpdir";
    }

    my $innodb_data_file_path = 
        get_option('innodb_data_file_path');

    # run ibbackup as a child process
    $cmdline = "$option_ibbackup_binary $options";

    # Only use --rebuild-indexes in the first xtrabackup call
    $cmdline_copy = $cmdline;
    if ($option_rebuild_indexes) {
        $cmdline = $cmdline . " --rebuild-indexes"
    }
    if ($option_rebuild_threads) {
        $cmdline = $cmdline . " --rebuild-threads=$option_rebuild_threads"
    }

    $now = current_time();
    print STDERR "\n$now  $prefix Starting ibbackup with command: $cmdline\n\n";
    $rcode = system("$cmdline");
    if ($rcode) {
        # failure
        die "\n$prefix ibbackup failed";
    }

    # We should not create ib_logfile files if we prepare for following incremental applies
    # Also we do not prepare ib_logfile if we applied incremental changes
    if (!( ($option_redo_only) or ($option_incremental_dir))) { 
        $cmdline = $cmdline_copy;
        $now = current_time();
        print STDERR "\n$now  $prefix Restarting xtrabackup with command: $cmdline\nfor creating ib_logfile*\n\n";
        $rcode = system("$cmdline");
        if ($rcode) {
            # failure
            die "\n$prefix xtrabackup (2nd execution) failed";
        }
    }

    # If we were applying an incremental change set, we need to make
    # sure non-InnoDB files and xtrabackup_* metainfo files are copied
    # to the full backup directory.
    if ( $option_incremental_dir ) {
	print STDERR "$prefix Starting to copy non-InnoDB files in '$option_incremental_dir'\n"; 
	print STDERR "$prefix to the full backup directory '$backup_dir'\n";
        my @skip_list = ('\.\.?','backup-my\.cnf','xtrabackup_logfile',
                        'xtrabackup_binary','xtrabackup_checkpoints',
                        '.*\.delta','.*\.meta','ib_logfile.*');
        copy_dir_recursively($option_incremental_dir, $backup_dir,
                             '^(' . join('|', @skip_list) . ')$', 1, 0);
    foreach my $c (parse_innodb_data_file_path($innodb_data_file_path)) {
            push(@skip_list, "\Q$c->{filename}\E");
        }
        cleanup_dir_recursively($backup_dir,
                '^(' . join('|', @skip_list, '.*\.ibd', 'undo[0-9]+') . ')$');
    }

}

#
# Loops until the specified file is created or the ibbackup process dies.
#
#
# If this function detects that the xtrabackup process has terminated, it also
# sets ibbackup_exit_code and resets ibbackup_pid to avoid trying to reap a
# non-existing process later
#
sub wait_for_ibbackup_file_create {

    my $suspend_file = shift;
    my $pid = -1;

    for (;;) {
        # check if the xtrabackup process is still alive _before_ checking if
        # the file exists to avoid a race condition when the file is created
        # and the process terminates right after we do the file check
        $pid = waitpid($ibbackup_pid, &WNOHANG);

        last if -e $suspend_file;

        if ($ibbackup_pid == $pid) {
            # The file doesn't exist, but the process has terminated
            $ibbackup_pid = '';
            die "The xtrabackup child process has died";
        }

        sleep 1;
    }

    if ($pid == $ibbackup_pid) {
        $ibbackup_exit_code = $CHILD_ERROR >> 8;
        $ibbackup_pid = '';
    }
}

#
# wait_for_ibbackup_suspend subroutine waits until ibbackup has suspended
# itself.
#
sub wait_for_ibbackup_suspend {
    my $suspend_file = shift;
    print STDERR "$prefix Waiting for ibbackup (pid=$ibbackup_pid) to suspend\n";
    print STDERR "$prefix Suspend file '$suspend_file'\n\n";
    wait_for_ibbackup_file_create($suspend_file);
    $now = current_time();
    open XTRABACKUP_PID, ">", "$option_tmpdir/$xtrabackup_pid_file"
        or die "Cannot open $option_tmpdir/$xtrabackup_pid_file: $OS_ERROR";
    print XTRABACKUP_PID $ibbackup_pid;
    close XTRABACKUP_PID;
    print STDERR "\n$now  $prefix Continuing after ibbackup has suspended\n";
}

#
# resume_ibbackup subroutine signals ibbackup to finish log copying by deleting
# $suspend_file.
#
sub resume_ibbackup {
    my $suspend_file = shift;
    unlink $suspend_file || die "Failed to delete '$suspend_file': $!";
}

#
# Wait until xtrabackup finishes copying log or dies.  The log copying is
# signaled by a sync file creation.  If this function detects that the
# xtrabackup process has terminated, it also sets ibbackup_exit_code and resets
# ibbackup_pid to avoid trying to reap a non-existing process later.
#
sub wait_for_ibbackup_log_copy_finish {
    my $suspend_file = shift;
    my $now = current_time();
    print STDERR "$now  $prefix Waiting for log copying to finish\n\n";

    wait_for_ibbackup_file_create($suspend_file);

    unlink $suspend_file || die "Failed to delete '$suspend_file': $!";
}

#
# wait for ibbackup to finish and return its exit code
#
sub wait_for_ibbackup_finish {
    if (!$ibbackup_pid) {
        # The process has already been reaped.
        unlink "$option_tmpdir/$xtrabackup_pid_file";
        return $ibbackup_exit_code;
    }

    $now = current_time();
    print STDERR "$now  $prefix Waiting for ibbackup (pid=$ibbackup_pid) to finish\n";

    # wait for ibbackup to finish
    waitpid($ibbackup_pid, 0);
    unlink "$option_tmpdir/$xtrabackup_pid_file";
    $ibbackup_pid = '';
    return $CHILD_ERROR >> 8;
}

#
# start_ibbackup subroutine spawns a child process running ibbackup
# program for backing up InnoDB tables and indexes.
#
sub start_ibbackup {
    my $options = '';
    my $cmdline = '';
    my $pid = undef;

    if ($option_defaults_file) {
        $options = $options . " --defaults-file=\"$option_defaults_file\" ";
    }

    if ($option_defaults_extra_file) {
        $options = $options . " --defaults-extra-file=\"$option_defaults_extra_file\" ";
    }

    if ($option_defaults_group) {
        $options = $options . " --defaults-group=\"$option_defaults_group\" ";
    }

    $options = $options . "--backup --suspend-at-end";

    if ($option_stream) {
        #(datadir) for 'xtrabackup_suspended' and 'xtrabackup_checkpoints'
        $options = $options . " --target-dir=" . $option_tmpdir;
    } else {
        $options = $options . " --target-dir=$backup_dir";
    }

    my $datadir = get_option('datadir');
    if (!has_option_in_config('datadir')) {
        $options .= " --datadir=\"$datadir\"";
    }

    my $innodb_log_file_size = get_option('innodb_log_file_size');
    if (!has_option_in_config('innodb_log_file_size')) {
        $options .= " --innodb_log_file_size=\"$innodb_log_file_size\"";
    }

    my $innodb_data_file_path = get_option('innodb_data_file_path');
    if (!has_option_in_config('innodb_data_file_path')) {
        $options .= " --innodb_data_file_path=\"$innodb_data_file_path\"";
    }

    if ($option_tmpdir) {
        $options .= " --tmpdir=$option_tmpdir";
    }

    # prepare command line for running ibbackup
    if ($option_throttle) {
        $options = $options . " --throttle=$option_throttle";
    }
    if ($option_log_copy_interval) {
        $options = $options . " --log-copy-interval=$option_log_copy_interval";
    }
    if ($option_sleep) {
        $options = $options . " --sleep=$option_sleep";
    }
    if ($option_compress) {
        $options = $options . " --compress";
        $options = $options . " --compress-threads=$option_compress_threads";
	if ($option_compress_chunk_size) {
	        $options = $options . " --compress-chunk-size=$option_compress_chunk_size";
	}
    }
    if ($option_encrypt) {
        $options = $options . " --encrypt=$option_encrypt";
        if ($option_encrypt_key) {
            $options = $options . " --encrypt-key=$option_encrypt_key";
        }
        if ($option_encrypt_key_file) {
            $options = $options . " --encrypt-key-file=$option_encrypt_key_file";
        }
        $options = $options . " --encrypt-threads=$option_encrypt_threads";
	if ($option_encrypt_chunk_size) {
	        $options = $options . " --encrypt-chunk-size=$option_encrypt_chunk_size";
	}
    }
    if ($option_use_memory) {
        $options = $options . " --use-memory=$option_use_memory";
    }
    if ($option_include) {
        $options = $options . " --tables='$option_include'";
    }
    if ($option_extra_lsndir) {
        $options = $options . " --extra-lsndir='$option_extra_lsndir'";
    }

    if ($option_incremental) {
        if($option_incremental_lsn) {
          $options = $options . " --incremental-lsn='$option_incremental_lsn'";
        } else {
          $options = $options . " --incremental-basedir='$incremental_basedir'";
        }
        if ($option_incremental_force_scan) {
          $options = $options . " --incremental-force-scan";
        } elsif ($have_changed_page_bitmaps) {
          $options = $options . " --suspend-at-start";
        }
    }

    if ($option_tables_file) {
        $options = $options . " --tables_file='$option_tables_file'";
    }
    if ($option_parallel) {
        $options = $options . " --parallel=$option_parallel";
    }
    if ($option_stream) {
        $options = $options . " --stream=$option_stream";
    }

    if ($option_close_files) {
        $options = $options . " --close-files";
    }

    if ($option_compact) {
	$options = $options . " --compact";
    }

    if ($option_databases) {
        if ($option_databases =~ /^\//) {
                $options = $options . " --databases_file='$option_databases'";
        } else {
                $options = $options . " --databases='$option_databases'";
        }
    }

    $cmdline = "$option_ibbackup_binary $options";

    # run ibbackup as a child process
    $now = current_time();
    print STDERR "\n$now  $prefix Starting ibbackup with command: $cmdline\n";
    if (defined($pid = fork)) {
        if ($pid) {
            # parent process
            $ibbackup_pid = $pid;
        } else {
            # child process
            exec($cmdline) || die "Failed to exec ibbackup: $!";
        }
    } else {
        die "failed to fork ibbackup child process: $!";
    }
}
                  

#
# parse_connection_options() subroutine parses connection-related command line
# options
#
sub parse_connection_options {
    my $con = shift;

    $con->{dsn} = 'dbi:mysql:';

    # this option has to be first
    if ($option_defaults_file) {
        $con->{dsn} .= ";mysql_read_default_file=$option_defaults_file";
    }

    if ($option_defaults_extra_file) {
        $con->{dsn} .= ";mysql_read_default_file=$option_defaults_extra_file";
    }

    $con->{dsn} .= ";mysql_read_default_group=xtrabackup";

    if ($option_mysql_password) {
        $con->{dsn_password} = "$option_mysql_password";
    }
    if ($option_mysql_user) {
        $con->{dsn_user} = "$option_mysql_user";
    }
    if ($option_mysql_host) {
        $con->{dsn} .= ";host=$option_mysql_host";
    }
    if ($option_mysql_port) {
        $con->{dsn} .= ";port=$option_mysql_port";
    }
    if ($option_mysql_socket) {
        $con->{dsn} .= ";mysql_socket=$option_mysql_socket";
    }
}

#
# mysql_connect subroutine connects to MySQL server
#
sub mysql_connect {
    my %con;
    my %args = (
                  # Defaults
                  abort_on_error => 1,
                  @_
               );

    $con{abort_on_error} = $args{abort_on_error};

    parse_connection_options(\%con);

    $now = current_time();
    print STDERR "$now  $prefix Connecting to MySQL server with DSN '$con{dsn}'" .
        (defined($con{dsn_user}) ? " as '$con{dsn_user}' " : "") .
        " (using password: ";
    if (defined($con{dsn_password})) {
        print STDERR "YES).\n";
    } else {
        print STDERR "NO).\n";
    }

    eval {
        $con{dbh}=DBI->connect($con{dsn}, $con{dsn_user},
                                 $con{dsn_password}, { RaiseError => 1 });
    };

    if ($EVAL_ERROR) {
        $con{connect_error}=$EVAL_ERROR;
    } else {
        $now = current_time();
        print STDERR "$now  $prefix Connected to MySQL server\n";
    }

    if ($args{abort_on_error}) {
        if (!$dbd_mysql_installed) {
            die "Failed to connect to MySQL server as " .
                "DBD::mysql module is not installed";
        } else {
            if (!$con{dbh}) {
                die "Failed to connect to MySQL server: " .
                    $con{connect_error};
            }
        }
    }

    if ($con{dbh}) {
        $con{dbh}->do("SET SESSION wait_timeout=2147483");
    }

    return %con;
}

#
# mysql_query subroutine send a query to MySQL server child process.
# Parameters:
#    con       mysql connection
#    query     query to execute
#
sub mysql_query {
  my ($con, $query) = @_;

  eval {
      if ($query eq 'SHOW VARIABLES') {
          $con->{vars} = $con->{dbh}->selectall_hashref('SHOW VARIABLES',
                                                        'Variable_name');
      } elsif ($query eq 'SHOW STATUS') {
          $con->{status} = $con->{dbh}->selectall_hashref('SHOW STATUS',
                                                        'Variable_name');
      } elsif ($query eq 'SHOW MASTER STATUS') {
          $con->{master_status} =
              $con->{dbh}->selectrow_hashref("SHOW MASTER STATUS");
      } elsif ($query eq 'SHOW SLAVE STATUS') {
          $con->{slave_status} =
              $con->{dbh}->selectrow_hashref("SHOW SLAVE STATUS");
      } elsif ($query eq 'SHOW PROCESSLIST' or
               $query eq "SHOW FULL PROCESSLIST") {
          $con->{processlist} =
              $con->{dbh}->selectall_hashref($query, "Id");
      } else {
          $con->{dbh}->do($query);
      }
  };
  if ($EVAL_ERROR) {
      die "\nError executing '$query': $EVAL_ERROR";
  }
}


sub get_mysql_vars {
  mysql_query($_[0], 'SHOW VARIABLES')
}

sub get_mysql_status {
  mysql_query($_[0], 'SHOW STATUS');
}

sub get_mysql_master_status {
  mysql_query($_[0], "SHOW MASTER STATUS");
}

sub get_mysql_slave_status {
  mysql_query($_[0], "SHOW SLAVE STATUS");
}

#
# mysql_close subroutine closes connection to MySQL server gracefully.
# 
sub mysql_close {
    my $con = shift;

    $con->{dbh}->disconnect();

    $now = current_time();
    print STDERR "$now  $prefix Connection to database server closed\n";
}

#
# write_binlog_info subroutine retrieves MySQL binlog position and
# saves it in a file. It also prints it to stdout.
#
sub write_binlog_info {
    my $con = shift;
    my $gtid;

    # get binlog position
    get_mysql_master_status($con);

    # get the name of the last binlog file and position in it
    # from the SHOW MASTER STATUS output
    my $filename = $con->{master_status}->{File};
    my $position = $con->{master_status}->{Position};
    my $mysql_gtid = defined($con->{vars}->{gtid_mode}) &&
                     $con->{vars}->{gtid_mode}->{Value} eq 'ON';
    my $mariadb_gtid = defined($con->{vars}->{gtid_current_pos});
    my @binlog_coordinates;
    my @binlog_info_content;

    # Do not create xtrabackup_binlog_info if binary log is disabled
    if (!defined($filename) or !defined($position)) {
        return;
    }

    if ($mariadb_gtid || !$mysql_gtid) {
        @binlog_coordinates = ("filename '$filename'", "position $position");
        @binlog_info_content = ($filename, $position);
    }

    $gtid = $con->{master_status}->{Executed_Gtid_Set};
    if (!defined($gtid)) {
        # Try to get MariaDB-style GTID
        get_mysql_vars($con);
        $gtid = $con->{vars}->{gtid_current_pos}->{Value};
    }

    if ($mariadb_gtid || $mysql_gtid) {
        push(@binlog_coordinates, "GTID of the last change '$gtid'");
        push(@binlog_info_content, $gtid);
    }

    $mysql_binlog_position = join(", ", @binlog_coordinates);

    # write binlog info file
    write_to_backup_file($binlog_info,
                         join("\t", @binlog_info_content) . "\n");
}

#
# write_galera_info subroutine retrieves MySQL Galera and
# saves it in a file. It also prints it to stdout.
#
sub write_galera_info {
    my $con = shift;
    my $state_uuid = undef;
    my $last_committed = undef;

    # When backup locks are supported by the server, we should skip creating
    # xtrabackup_galera_info file on the backup stage, because
    # wsrep_local_state_uuid and wsrep_last_committed will be inconsistent
    # without blocking commits. The state file will be created on the prepare
    # stage using the WSREP recovery procedure.
    if ($have_backup_locks) {
        return;
    }

    # get binlog position
    get_mysql_status($con);

    # MariaDB Galera Cluster uses capitalized wsrep_* status variables
    if (defined($con->{status}->{Wsrep_local_state_uuid})) {
        $state_uuid = $con->{status}->{Wsrep_local_state_uuid}->{Value};
    } elsif (defined($con->{status}->{wsrep_local_state_uuid})) {
        $state_uuid = $con->{status}->{wsrep_local_state_uuid}->{Value};
    }

    if (defined($con->{status}->{Wsrep_last_committed})) {
        $last_committed = $con->{status}->{Wsrep_last_committed}->{Value};
    } elsif (defined($con->{status}->{wsrep_last_committed})) {
        $last_committed = $con->{status}->{wsrep_last_committed}->{Value};
    }

    if (!defined($state_uuid) || !defined($last_committed)) {
        die "Failed to get master wsrep state from SHOW STATUS.";
    }

    write_to_backup_file("$galera_info", "$state_uuid" .
      ":" . "$last_committed" . "\n");
}

#
# Flush and copy the current binary log file into the backup, if GTID is enabled
#
sub write_current_binlog_file {
    my $con = shift;
    my $gtid_exists= 0;

    if (!defined($con->{master_status})) {
        get_mysql_master_status($con);
    }

    if (defined($con->{master_status}->{Executed_Gtid_Set})) {
        # MySQL >= 5.6
        if ($con->{master_status}->{Executed_Gtid_Set} ne '') {
            $gtid_exists = 1;
        }
    } elsif (defined($con->{vars}->{gtid_binlog_state})) {
         # MariaDB >= 10.0
        if ($con->{vars}->{gtid_binlog_state}->{Value} ne '') {
            $gtid_exists = 1;
        }
    }

    if ($gtid_exists) {
        my $log_bin_dir;
        my $log_bin_file;

        mysql_query($con, "FLUSH BINARY LOGS");
        get_mysql_master_status($con);

        $log_bin_file = $con->{master_status}->{File};
        if (defined($con->{vars}->{log_bin_basename})) {
            # MySQL >= 5.6
            $log_bin_dir =
                File::Basename::dirname($con->{vars}->{log_bin_basename}->{Value});
        } else {
            # MariaDB >= 10.0
            # log_bin_basename does not exist in MariaDB, fallback to datadir
             $log_bin_dir = $con->{vars}->{datadir}->{Value};
        }

        if (!defined($log_bin_file) || !defined($log_bin_dir)) {
            die "Failed to get master binlog coordinates from SHOW MASTER STATUS";
        }

        backup_file("$log_bin_dir", "$log_bin_file",
                    "$backup_dir/$log_bin_file");
    }
}


# 
# write_slave_info subroutine retrieves MySQL binlog position of the
# master server in a replication setup and saves it in a file. It
# also saves it in $msql_slave_position variable.
#
sub write_slave_info {
    my $con = shift;

    my @lines;
    my @info_lines;

    # get slave status
    get_mysql_slave_status($con);
    get_mysql_vars($con);

    my $master = $con->{slave_status}->{Master_Host};
    my $filename = $con->{slave_status}->{Relay_Master_Log_File};
    my $position = $con->{slave_status}->{Exec_Master_Log_Pos};

    if (!defined($master) || !defined($filename) || !defined($position)) {
        my $now = current_time();

        print STDERR "$now  $prefix Failed to get master binlog coordinates " .
            "from SHOW SLAVE STATUS\n";
        print STDERR "$now  $prefix This means that the server is not a " .
            "replication slave. Ignoring the --slave-info option\n";

        return;
    }

    my $gtid_executed = $con->{slave_status}->{Executed_Gtid_Set};

    # Print slave status to a file.
    # If GTID mode is used, construct a CHANGE MASTER statement with
    # MASTER_AUTO_POSITION and correct a gtid_purged value.
    if (defined($gtid_executed) && $gtid_executed ne '') {
        # MySQL >= 5.6 with GTID enabled
        $gtid_executed =~ s/\n/ /;
        write_to_backup_file("$slave_info",
                             "SET GLOBAL gtid_purged='$gtid_executed';\n" .
                             "CHANGE MASTER TO MASTER_AUTO_POSITION=1\n");
        $mysql_slave_position = "master host '$master', ".
            "purge list '$gtid_executed'";
    } elsif (defined($con->{vars}->{gtid_slave_pos}) &&
             $con->{vars}->{gtid_slave_pos}->{Value} ne '') {
        # MariaDB >= 10.0 with GTID enabled
        write_to_backup_file("$slave_info",
                             "CHANGE MASTER TO master_use_gtid = " .
                             "slave_pos\n"); # or current_pos ??
        $mysql_slave_position = "master host '$master', " .
            "gtid_slave_pos " . $con->{vars}->{gtid_slave_pos}->{Value};
    } else {
        write_to_backup_file("$slave_info",
                             "CHANGE MASTER TO MASTER_LOG_FILE='$filename', " .
                             "MASTER_LOG_POS=$position\n");

        $mysql_slave_position = "master host '$master', " .
            "filename '$filename', " .
            "position $position";
    }
}

sub eat_sql_whitespace {
    my ($query) = @_;

    while (1) {
        if ($query =~ m/^\/\*/) {
            $query =~ s/^\/\*.*?\*\///s;
        } elsif ($query =~ m/^[ \t\n\r]/s) {
            $query =~ s/^[ \t\n\r]+//s;
        } elsif ($query =~ m/^\(/) {
            $query =~ s/^\(//;
        } else {
            return $query;
        }
    }

}


sub is_query {
    my ($query) = @_;

    $query = eat_sql_whitespace($query);
    if ($query =~
        m/^(insert|update|delete|replace|alter|load|select|do|handler|call|execute|begin)/i) {
        return 1;
    }

    return 0;
}


sub is_select_query {
    my ($query) = @_;

    $query = eat_sql_whitespace($query);
    if ($query =~ m/^select/i) {
        return 1;
    }

    return 0;
}


sub is_update_query {
    my ($query) = @_;

    $query = eat_sql_whitespace($query);
    if ($query =~ m/^(insert|update|delete|replace|alter|load)/i) {
        return 1;
    }

    return 0;
}


sub have_queries_to_wait_for {
    my ($con, $threshold) = @_;

    $now = current_time();

    mysql_query($con, "SHOW FULL PROCESSLIST");

    my $processlist = $con->{processlist};

    while (my ($id, $process) = each %$processlist) {
        if (defined($process->{Info}) &&
            $process->{Time} >= $threshold &&
            (($option_lock_wait_query_type eq "all" &&
             is_query($process->{Info})) ||
             is_update_query($process->{Info}))) {
            print STDERR "\n$now  $prefix Waiting for query $id (duration " .
                            "$process->{Time} sec): $process->{Info}\n";
            return 1;
        }
    }

    return 0;
}


sub kill_long_queries {
    my ($con, $timeout) = @_;

    $now = current_time();

    mysql_query($con, "SHOW FULL PROCESSLIST");

    my $processlist = $con->{processlist};

    while (my ($id, $process) = each %$processlist) {
        if (defined($process->{Info}) &&
            $process->{Time} >= $timeout &&
            (($option_kill_long_query_type eq "all" &&
              is_query($process->{Info})) ||
             is_select_query($process->{Info}))) {
            print STDERR "\n$now  $prefix Killing query $id (duration " .
                           "$process->{Time} sec): $process->{Info}\n";
            mysql_query($con, "KILL $id");
        }
    }
}


sub wait_for_no_updates {
    my ($con, $timeout, $threshold) = @_;
    my $start_time = time();

    while (time() <= $start_time + $timeout) {
        if (!(have_queries_to_wait_for($con, $threshold))) {
            return;
        }
        sleep(1);
    }

    die "Unable to obtain lock. Please try again.";
}


sub start_query_killer {

    my ($kill_timeout, $pcon) = @_;
    my $start_time = time();
    my $pid = fork();

    if ($pid) {
        # parent process
        $query_killer_pid = $pid;
    } else {
        # child process
        my $end = 0;
        local $SIG{HUP} = sub { $end = 1 };

        $pcon->{dbh}->{InactiveDestroy} = 1;
        kill('USR1', $innobackupex_pid);

        sleep($kill_timeout);

        my %con = mysql_connect(abort_on_error => 1);

        while (!$end) {
            kill_long_queries(\%con, time() - $start_time);
            sleep(1);
        }

        mysql_close(\%con);

        exit(0);
    }
}

sub stop_query_killer {
    if (defined($query_killer_pid)) {
        kill 'HUP' => $query_killer_pid;
        waitpid($query_killer_pid, 0);
        undef $query_killer_pid;
        print STDERR "Query killing process is finished\n";
    }
}


#
# mysql_lock_tables subroutine acquires either a backup tables lock, if
# supported by the server, or a global read lock (FLUSH TABLES WITH READ LOCK)
# otherwise.
#
sub mysql_lock_tables {
    my $con = shift;
    my $queries_hash_ref;

    if ($have_lock_wait_timeout) {
        # Set the maximum supported session value for lock_wait_timeout to
        # prevent unnecessary timeouts when the global value is changed from the
        # default
        mysql_query($con, "SET SESSION lock_wait_timeout=31536000");
    }

    if ($have_backup_locks) {
        $now = current_time();
        print STDERR "$now  $prefix Executing LOCK TABLES FOR BACKUP...\n";

        mysql_query($con, "LOCK TABLES FOR BACKUP");

        $now = current_time();
        print STDERR "$now  $prefix Backup tables lock acquired\n";

        return;
    }

    if ($option_lock_wait_timeout) {
        wait_for_no_updates($con, $option_lock_wait_timeout,
                            $option_lock_wait_threshold);
    }

    $now = current_time();
    print STDERR "$now  $prefix Executing FLUSH TABLES WITH READ LOCK...\n";

    my $query_killer_init = 0;

    # start query killer process
    if ($option_kill_long_queries_timeout) {
        local $SIG{'USR1'} = sub { $query_killer_init = 1 };
        start_query_killer($option_kill_long_queries_timeout, $con);
        while (!$query_killer_init) {
            usleep(1);
        }
    }

    if ($have_galera_enabled) {
        mysql_query($con, "SET SESSION wsrep_causal_reads=0");
    }

    mysql_query($con, "FLUSH TABLES WITH READ LOCK");

    $now = current_time();
    print STDERR "$now  $prefix All tables locked and flushed to disk\n";

    # stop query killer process
    if ($option_kill_long_queries_timeout) {
        stop_query_killer();
    }
}

#
# If backup locks are used, execete LOCK BINLOG FOR BACKUP.
#
sub mysql_lock_binlog {
    if ($have_backup_locks) {
        $now = current_time();
        print STDERR "$now  $prefix Executing LOCK BINLOG FOR BACKUP...\n";

        mysql_query($_[0], "LOCK BINLOG FOR BACKUP");
    }
}

#
# mysql_unlock_all subroutine releases either global read lock acquired with
# FTWRL and the binlog lock acquired with LOCK BINLOG FOR BACKUP, depending on
# the locking strategy being used
#
sub mysql_unlock_all {
    my $con = shift;

    if ($option_debug_sleep_before_unlock) {
        my $now = current_time();
        print STDERR "$now  $prefix Debug sleep for ".
            "$option_debug_sleep_before_unlock seconds\n";
        sleep $option_debug_sleep_before_unlock;
    }

    if ($have_backup_locks) {
        $now = current_time();
        print STDERR "$now  $prefix Executing UNLOCK BINLOG\n";
        mysql_query($con, "UNLOCK BINLOG");

        $now = current_time();
        print STDERR "$now  $prefix Executing UNLOCK TABLES\n";
        mysql_query($con, "UNLOCK TABLES");
    } else {
        mysql_query ($con, "UNLOCK TABLES");
    }

    $now = current_time();
    print STDERR "$now  $prefix All tables unlocked\n";
}

#
# require_external subroutine checks that an external program is runnable
# via the shell. This is tested by calling the program with the
# given arguments. It is checked that the program returns 0 and does 
# not print anything to stderr. If this check fails, this subroutine exits.
#    Parameters:
#       program      pathname of the program file
#       args         arguments to the program
#       pattern      a string containing a regular expression for finding 
#                    the program version.
#                    this pattern should contain a subpattern enclosed
#                    in parentheses which is matched with the version.
#       version_ref  a reference to a variable where the program version
#                    string is returned. Example "2.0-beta2".
#
sub require_external {
    my $program = shift;
    my $args = shift;
    my $pattern = shift;
    my $version_ref = shift;
    my @lines;
    my $tmp_stdout = tmpnam();
    my $tmp_stderr = tmpnam();
    my $rcode;
    my $error;

    $rcode = system("$program $args >$tmp_stdout 2>$tmp_stderr");
    if ($rcode) {
        $error = $!;
    }
    my $stderr = `cat $tmp_stderr | grep -v ^Warning`;
    unlink $tmp_stderr;
    if ($stderr ne '') {
        # failure
        unlink $tmp_stdout;
        die "Couldn't run $program: $stderr";
    } elsif ($rcode) {
        # failure
        unlink $tmp_stdout;
        die "Couldn't run $program: $error";
    }

    # success
    my $stdout = `cat $tmp_stdout`;
    unlink $tmp_stdout;
    @lines = split(/\n|;/,$stdout);
    print STDERR "$prefix Using $lines[0]\n";

    # get version string from the first output line of the program
    ${$version_ref} = '';
    if ($lines[0] =~ /$pattern/) {
        ${$version_ref} = $1;
    }
}

#
# init subroutine initializes global variables and performs some checks on the
# system we are running on.
#
sub init {
    my $mysql_version = '';
    my $ibbackup_version = '';
    my $run = '';

    # print some instructions to the user
    if (!$option_apply_log && !$option_copy_back && !$option_move_back
        && !$option_decrypt && !$option_decompress) {
        $run = 'backup';
    } elsif ($option_decrypt || $option_decompress) {
        $run = 'decryption and decompression';
    } elsif ($option_copy_back) {
        $run = 'copy-back';
    } elsif ($option_move_back) {
        $run = 'move-back';
    } else {
        $run = 'apply-log';
    }

    my $now = current_time();

    print STDERR "$now  $prefix Starting the $run operation\n\n";

    print STDERR "IMPORTANT: Please check that the $run run completes successfully.\n";
    print STDERR "           At the end of a successful $run run $innobackup_script\n";
    print STDERR "           prints \"completed OK!\".\n\n";

    if ($option_apply_log || $option_copy_back || $option_move_back) {
        # read server configuration file
        read_config_file(\%config);
    } elsif ($option_backup) {
        # we are making a backup, read server configuration from SHOW VARIABLES
        get_mysql_vars(\%mysql);

        # and make sure datadir value is the same in configuration file
        read_config_file(\%config);

        my $server_val = $mysql{vars}->{datadir}->{Value};

        if (has_option_in_config('datadir')) {
            my $config_val = $config{$option_defaults_group}{datadir};

            if ($server_val ne $config_val &&
                # Try to canonicalize paths
                realpath($server_val) ne realpath($config_val)) {

                die "option 'datadir' has different values:\n" .
                    "  '$config_val' in defaults file\n" .
                    "  '$server_val' in SHOW VARIABLES\n"
                }
        }

        $mysql_server_version = $mysql{vars}->{version}->{Value};
        print STDERR "$prefix  Using server version $mysql_server_version\n";
        print STDERR "\n";
    }

    if (!$option_tmpdir && ($option_backup || $option_apply_log) &&
        has_option('tmpdir')) {
        $option_tmpdir = get_option('tmpdir');
        # tmpdir can be a colon-separated list of multiple directories
        $option_tmpdir = (split(/:/, $option_tmpdir))[0];
    }

    if ($option_backup) {
        # we are making a backup, create a new backup directory
        $backup_dir = File::Spec->rel2abs(make_backup_dir());
        print STDERR "$prefix Created backup directory $backup_dir\n";
        if (!$option_stream) {
            $work_dir = $backup_dir;
        } else {
            $work_dir = $option_tmpdir;
        }
        $backup_config_file = $work_dir . '/backup-my.cnf';
        $binlog_info = $work_dir . '/xtrabackup_binlog_info';
        $galera_info = $work_dir . '/xtrabackup_galera_info';
        $slave_info = $work_dir . '/xtrabackup_slave_info';
        $backup_history = $work_dir . '/xtrabackup_info';
        write_backup_config_file($backup_config_file);

        if (!$option_extra_lsndir) {
            $option_extra_lsndir = $option_tmpdir;
            $history_delete_checkpoints = 1;
        }

        foreach (@xb_suspend_files) {
            my $suspend_file = $work_dir . "$_";
            if ( -e "$suspend_file" ) {
                print STDERR
                    "WARNING : A left over instance of " .
                    "suspend file '$suspend_file' was found.\n";
                unlink "$suspend_file"
                    || die "Failed to delete '$suspend_file': $!";
            }
        }

        if ( -e "$option_tmpdir/$xtrabackup_pid_file" ) {
            print STDERR "WARNING : A left over instance of xtrabackup pid " .
                         "file '$option_tmpdir/$xtrabackup_pid_file' " .
                         "was found.\n";
            unlink $option_tmpdir . "/" . $xtrabackup_pid_file || 
                die "Failed to delete " .
                    "'$option_tmpdir/$xtrabackup_pid_file': $!";
        }
    }
}


#
# write_backup_config_file subroutine creates a backup options file for
# ibbackup program. It writes to the file only those options that
# are required by ibbackup.
#    Parameters:
#       filename  name for the created options file
#
sub write_backup_config_file {
    my $filename = shift;

    my @option_names = (
        "innodb_checksum_algorithm",
        "innodb_log_checksum_algorithm",
        "innodb_data_file_path",
        "innodb_log_files_in_group",
        "innodb_log_file_size",
        "innodb_fast_checksum",
        "innodb_page_size",
        "innodb_log_block_size",
        "innodb_undo_directory",
        "innodb_undo_tablespaces"
        );

    my $options_dump = "# This MySQL options file was generated by $innobackup_script.\n\n" .
          "# The MySQL server\n" .
          "[mysqld]\n";

    my $option_name;
    foreach $option_name (@option_names) {
        if (has_option($option_name)) {
            my $option_value = get_option($option_name);
            $options_dump .= "$option_name=$option_value\n";
        }
    }
    if (has_option('innodb_doublewrite_file')) {
        my $option_value = (split(/\/+/,
                                  get_option('innodb_doublewrite_file')))[-1];

        if (defined($option_value)) {
            $options_dump .= "innodb_doublewrite_file=" . $option_value . "\n";
        }
    }

    write_to_backup_file("$filename", "$options_dump");
}


#
# check_args subroutine checks command line arguments. If there is a problem,
# this subroutine prints error message and exits.
#
sub check_args {
    my $i;
    my $rcode;
    my $buf;
    my $perl_version;

    # check the version of the perl we are running
    if (!defined $^V) {
        # this perl is prior to 5.6.0 and uses old style version string
        my $required_version = $required_perl_version_old_style;
        if ($] lt $required_version) {
            print STDERR "$prefix Warning: " . 
                "Your perl is too old! Innobackup requires\n";
            print STDERR "$prefix Warning: perl $required_version or newer!\n";
        }
    }

    # read command line options
    $rcode = GetOptions('compress' => \$option_compress,
                        'decompress' => \$option_decompress,
                        'compress-threads=i' => \$option_compress_threads,
                        'compress-chunk-size=s' => \$option_compress_chunk_size,
                        'encrypt=s' => \$option_encrypt,
                        'decrypt=s' => \$option_decrypt,
                        'encrypt-key=s' => \$option_encrypt_key,
                        'encrypt-key-file=s' => \$option_encrypt_key_file,
                        'encrypt-threads=i' => \$option_encrypt_threads,
                        'encrypt-chunk-size=s' => \$option_encrypt_chunk_size,
                        'help' => \$option_help,
                        'history:s' => \$option_history,
                        'version' => \$option_version,
                        'throttle=i' => \$option_throttle,
                        'log-copy-interval=i', \$option_log_copy_interval,
                        'sleep=i' => \$option_sleep,
                        'apply-log' => \$option_apply_log,
                        'redo-only' => \$option_redo_only,
                        'copy-back' => \$option_copy_back,
                        'move-back' => \$option_move_back,
                        'include=s' => \$option_include,
                        'databases=s' => \$option_databases,
                        'tables-file=s', => \$option_tables_file,
                        'use-memory=s' => \$option_use_memory,
                        'export' => \$option_export,
                        'password:s' => \$option_mysql_password,
                        'user=s' => \$option_mysql_user,
                        'host=s' => \$option_mysql_host,
                        'port=s' => \$option_mysql_port,
                        'defaults-group=s' => \$option_defaults_group,
                        'slave-info' => \$option_slave_info,
                        'galera-info' => \$option_galera_info,
                        'socket=s' => \$option_mysql_socket,
                        'no-timestamp' => \$option_no_timestamp,
                        'defaults-file=s' => \$option_defaults_file,
                        'defaults-extra-file=s' => \$option_defaults_extra_file,
                        'incremental' => \$option_incremental,
                        'incremental-basedir=s' => \$option_incremental_basedir,
                        'incremental-force-scan' => \$option_incremental_force_scan,
                        'incremental-history-name=s' => \$option_incremental_history_name,
                        'incremental-history-uuid=s' => \$option_incremental_history_uuid,
                        'incremental-lsn=s' => \$option_incremental_lsn,
                        'incremental-dir=s' => \$option_incremental_dir,
                        'extra-lsndir=s' => \$option_extra_lsndir,
                        'stream=s' => \$option_stream,
                        'rsync' => \$option_rsync,
                        'tmpdir=s' => \$option_tmpdir,
                        'no-lock' => \$option_no_lock,
                        'ibbackup=s' => \$option_ibbackup_binary,
                        'parallel=i' => \$option_parallel,
                        'safe-slave-backup' => \$option_safe_slave_backup,
                        'safe-slave-backup-timeout=i' => \$option_safe_slave_backup_timeout,
                        'close-files' => \$option_close_files,
                        'compact' => \$option_compact,
                        'rebuild-indexes' => \$option_rebuild_indexes,
                        'rebuild-threads=i' => \$option_rebuild_threads,
                        'debug-sleep-before-unlock=i' =>
                        \$option_debug_sleep_before_unlock,
                        'kill-long-queries-timeout=i' =>
                        \$option_kill_long_queries_timeout,
                        'kill-long-query-type=s' =>
                        \$option_kill_long_query_type,
                        'lock-wait-timeout=i' => \$option_lock_wait_timeout,
                        'lock-wait-threshold=i' => \$option_lock_wait_threshold,
                        'lock-wait-query-type=s' =>
                        \$option_lock_wait_query_type,
                        'version-check!' => \$option_version_check,
                        'force-non-empty-directories' =>
                        \$option_force_non_empty_dirs
    );

    if ($option_help) {
        # print help text and exit
        usage();
        exit(0);
    }
    if ($option_version) {
        # print program version and copyright
        print_version();
        exit(0);
    }

    if (@ARGV == 0) {
        die "You must specify the backup directory.\n";
    } elsif (@ARGV > 1) {
        die "Too many command line arguments\n";
    }

    if (!$rcode) {
        # failed to read options
        die "Bad command line arguments\n";
    }

    if ($option_defaults_file && $option_defaults_extra_file) {
        die "--defaults-file and --defaults-extra-file " .
            "options are mutually exclusive";
    }

    my @mixed_options = ();

    if ($option_decompress) {
        push(@mixed_options, "--decompress");
    } elsif ($option_decrypt) {
        push(@mixed_options, "--decrypt");
    }

    if ($option_copy_back) {
        push(@mixed_options, "--copy-back");
    }

    if ($option_move_back) {
        push(@mixed_options, "--move-back");
    }

    if ($option_apply_log) {
        push(@mixed_options, "--apply-log");
    }

    if (scalar(@mixed_options) > 1) {
        die join(' and ', @mixed_options) .
            " are mutually exclusive";
    }

    if ($option_compress == 0) {
        # compression level no specified, use default level
        $option_compress = $default_compression_level;
    } 

    if ($option_compress == 999) {
        # compress option not given in the command line
        $option_compress = 0;
    }

    # validate lock-wait-query-type and kill-long-query-type values
    if (!(grep {$_ eq $option_lock_wait_query_type} qw/all update/)) {
        die "Wrong value of lock-wait-query-type. ".
            "Possible values are all|update, but $option_lock_wait_query_type ".
            "is specified.";
    }
    if (!(grep {$_ eq $option_kill_long_query_type} qw/all select/)) {
        die "Wrong value of kill-long-query-type. ".
            "Possible values are all|select, but $option_kill_long_query_type ".
            "is specified.";
    }

    if ($option_parallel && $option_parallel < 1) {
        die "--parallel must be a positive value.\n";
    }

    if ($option_stream eq 'tar') {
      $stream_cmd = 'tar chf -';
    } elsif ($option_stream eq 'xbstream') {
      $stream_cmd = 'xbstream -c';
    }

    if ($option_encrypt) {
      $encrypt_cmd = "xbcrypt --encrypt-algo=$option_encrypt";
      if ($option_encrypt_key) {
        $encrypt_cmd = $encrypt_cmd . " --encrypt-key=$option_encrypt_key";
      }
      if ($option_encrypt_key_file) {
        $encrypt_cmd = $encrypt_cmd . " --encrypt-key-file=$option_encrypt_key_file";
      }
      if ($option_encrypt_chunk_size) {
        $encrypt_cmd = $encrypt_cmd . " --encrypt-chunk-size=$option_encrypt_chunk_size";
      }
    }

    if (!$option_incremental &&
        ($option_incremental_lsn ne '' ||
         $option_incremental_basedir ne '' ||
         $option_incremental_history_name ne '' ||
         $option_incremental_history_uuid ne '' )) {
        die "--incremental-lsn, --incremental-basedir, " .
            "--incremental-history-name and --incremental-history-uuid " .
            "require the --incremental option.\n"
    }

    if (!$option_apply_log && !$option_copy_back && !$option_move_back
        && !$option_decrypt && !$option_decompress) {
        # we are making a backup, get backup root directory
        $option_backup = "1";
        $backup_root = $ARGV[0];
        if ($option_incremental && $option_incremental_lsn eq '' &&
            $option_incremental_history_name eq '' &&
            $option_incremental_history_uuid eq '') {
            if ($option_incremental_basedir ne '') {
                $incremental_basedir = $option_incremental_basedir;
            } else {
                my @dirs = `ls -t $backup_root`;
                my $inc_dir = $dirs[0];
                chomp($inc_dir);
                $incremental_basedir = File::Spec->catfile($backup_root, $inc_dir);
            }
        }
    } else {
        # get backup directory
        $backup_dir = File::Spec->rel2abs($ARGV[0]);
    }        

    if ($option_slave_info) {
        if ($option_no_lock and !$option_safe_slave_backup) {
	  die "--slave-info is used with --no-lock but without --safe-slave-backup. The binlog position cannot be consistent with the backup data.\n";
	}
    }

    if ($option_rsync && $option_stream) {
        die "--rsync doesn't work with --stream\n";
    }

    if ($option_decompress) {
      if (system("which qpress &>/dev/null") >> 8 != 0) {
        die "--decompress requires qpress\n";
      }
    }

    print STDERR "\n";

    parse_databases_option_value();
    parse_tables_file_option_value($option_tables_file);
}


#
# make_backup_dir subroutine creates a new backup directory and returns
# its name.
#
sub make_backup_dir {
    my $dir;

    # create backup directory
    $dir = $backup_root;
    if ($option_stream) {
        return $dir;
    }

    $dir .= '/' . strftime("%Y-%m-%d_%H-%M-%S", localtime())
       unless $option_no_timestamp;
    mkdir($dir, 0777) || die "Failed to create backup directory $dir: $!";

    return $dir;
}


#
# create_path_if_needed subroutine checks that all components
# in the given relative path are directories. If the
# directories do not exist, they are created.
#    Parameters:
#       root           a path to the root directory of the relative pathname
#       relative_path  a relative pathname (a reference to an array of 
#                      pathname components) 
#
sub create_path_if_needed {
    my $root = shift;
    my $relative_path = shift;
    my $path;

    $path = $root;
    foreach $a (@{$relative_path}) {
        $path = $path . "/" . $a;
        if (! -d $path) {
            # this directory does not exist, create it !
            mkdir($path, 0777) || die "Failed to create backup directory: $!";
        }
    }
}


#
# remove_from_array subroutine removes excluded element from the array.
#    Parameters:
#       array_ref   a reference to an array of strings
#       excluded   a string to be excluded from the copy
#  
sub remove_from_array {
    my $array_ref = shift;
    my $excluded = shift;
    my @copy = ();
    my $size = 0;

    foreach my $str (@{$array_ref}) {
        if ($str ne $excluded) {
            $copy[$size] = $str;
            $size = $size + 1;
        }
    }
    @{$array_ref} = @copy;
}


#
# backup_files subroutine copies .frm, .isl, .MRG, .MYD and .MYI files to 
# backup directory.
#
sub backup_files {
    my $prep_mode = shift;
    my $source_dir = get_option('datadir');
    my $buffer_pool_filename = get_option_safe('innodb_buffer_pool_filename',
                                              '');
    my @list;
    my $file;
    my $database;
    my $wildcard = '*.{frm,isl,MYD,MYI,MAD,MAI,MRG,TRG,TRN,ARM,ARZ,CSM,CSV,opt,par}';
    my $rsync_file_list;
    my $operation;
    my $rsync_tmpfile_pass1 = "$option_tmpdir/xtrabackup_rsyncfiles_pass1";
    my $rsync_tmpfile_pass2 = "$option_tmpdir/xtrabackup_rsyncfiles_pass2";

    # prep_mode will pre-copy the data, so that rsync is faster the 2nd time
    # saving time while all tables are locked.
    # currently only rsync mode is supported for prep.
    if ($prep_mode and !$option_rsync) {
        return;
    }

    if ($option_rsync) {
	if ($prep_mode) {
	    $rsync_file_list = $rsync_tmpfile_pass1;
	} else {
	    $rsync_file_list = $rsync_tmpfile_pass2;
	}
	open(RSYNC, ">$rsync_file_list")
	    || die "Can't open $rsync_file_list for writing: $!";
    }

    opendir(DIR, $source_dir) 
        || die "Can't open directory '$source_dir': $!";
    $now = current_time();
    if ($prep_mode) {
	$operation = "a prep copy of";
    } else {
	$operation = "to backup";
    }
    print STDERR "\n$now  $prefix Starting $operation non-InnoDB tables and files\n";
    print STDERR "$prefix in subdirectories of '$source_dir'\n";
    # loop through all database directories
    while (defined($database = readdir(DIR))) {
        my $print_each_file = 0;
        my $file_c;
        my @scp_files;
        # skip files that are not database directories
        if ($database eq '.' || $database eq '..') { next; }
        next unless -d "$source_dir/$database";
	     next unless check_if_required($database);
        
        if (!$option_stream) {
            if (! -e "$backup_dir/$database") {
                # create database directory for the backup
                mkdir("$backup_dir/$database", 0777)
                    || die "Couldn't create directory '$backup_dir/$database': $!";
            }
        }

        # copy files of this database
	opendir(DBDIR, "$source_dir/$database");
	@list = grep(/\.(frm|isl|MYD|MYI|MAD|MAI|MRG|TRG|TRN|ARM|ARZ|CSM|CSV|opt|par)$/, readdir(DBDIR));
	closedir DBDIR;
        $file_c = @list;
        if ($file_c <= $backup_file_print_limit) {
            $print_each_file = 1;
        } else {
            print STDERR "$prefix Backing up files " . 
                "'$source_dir/$database/$wildcard' ($file_c files)\n";
        }

        if ($file_c == 0 && $option_stream) {
            # Stream/encrypt empty directories by backing up a fake empty
            # db.opt file, so that empty databases are created in the backup
            mkdir("$option_tmpdir/$database") ||
                die "Failed to create directory $option_tmpdir/$database: $!";

            open XTRABACKUP_FH, ">", "$option_tmpdir/$database/db.opt"
                or die "Cannot create file $option_tmpdir/db.opt: $!";
            close XTRABACKUP_FH;

            backup_file_via_stream("$option_tmpdir", "$database/db.opt");

            unlink("$option_tmpdir/$database/db.opt") ||
                die "Failed to remove file $database/db.opt: $!";
            rmdir("$option_tmpdir/$database") ||
                die "Failed to remove directory $database: $!";
        }

        foreach $file (@list) {
            next unless check_if_required($database, $file);

	    if($option_include) {
                my $table = get_table_name($file);
                my $table_part = get_table_name_with_part_suffix($file);

                if (!("$database.$table_part" =~ /$option_include/ ||
                      "$database.$table" =~ /$option_include/)) {

                    if ($print_each_file) {
                        print STDERR "$database.$file is skipped because it does not match '$option_include'.\n";
                    }
                    next;
                }
	    }
               
            if ($print_each_file) {
                print STDERR "$prefix Backing up file '$source_dir/$database/$file'\n";
            }

	    if ($option_rsync) {
		print RSYNC "$database/$file\n";
		if (!$prep_mode) {
		    $rsync_files_hash{"$database/$file"} = 1;
		}
            } else {
              backup_file("$source_dir", "$database/$file", "$backup_dir/$database/$file")
            }
        }
    }
    closedir(DIR);

    if ($option_rsync) {
        foreach my $dump_name ($buffer_pool_filename, 'ib_lru_dump') {
            if ($dump_name ne '' && -e "$source_dir/$dump_name") {
                print RSYNC "$dump_name\n";
                if (!$prep_mode) {
                    $rsync_files_hash{"$dump_name"} = 1;
                }
            }
        }
	close(RSYNC);

	# do the actual rsync now
	$now = current_time();
	my $rsync_cmd = "rsync -t \"$source_dir\" --files-from=\"$rsync_file_list\" \"$backup_dir\"";
	print STDERR "$now Starting rsync as: $rsync_cmd\n";

	# ignore errors in the prep mode, since we are running without lock,
	# so some files may have disappeared.
	if (system("$rsync_cmd") && !$prep_mode) {
	    die "rsync failed: $!";
	}

	$now = current_time();
	print STDERR "$now rsync finished successfully.\n";

	# Remove from $backup_dir files that have been removed between first and
	# second passes. Cannot use "rsync --delete" because it does not work
	# with --files-from.
	if (!$prep_mode && !$option_no_lock) {
	    open(RSYNC, "<$rsync_tmpfile_pass1")
		|| die "Can't open $rsync_tmpfile_pass1 for reading: $!";

	    while (<RSYNC>) {
		chomp;
		if (!exists $rsync_files_hash{$_}) {
		    print STDERR "Removing '$backup_dir/$_'\n";
		    unlink "$backup_dir/$_";
		}
	    }

	    close(RSYNC);
	    unlink "$rsync_tmpfile_pass1" || \
		die "Failed to delete $rsync_tmpfile_pass1: $!";
	    unlink "$rsync_tmpfile_pass2" || \
		die "Failed to delete $rsync_tmpfile_pass2: $!";
	}
    }

    if ($prep_mode) {
	$operation = "a prep copy of";
    } else {
	$operation = "backing up";
    }
    $now = current_time();
    print STDERR "$now  $prefix Finished $operation non-InnoDB tables and files\n\n";
 }


#
# file_to_array subroutine reads the given text file into an array and
# stores each line as an element of the array. The end-of-line
# character(s) are removed from the lines stored in the array.
#    Parameters:
#       filename   name of a text file
#       lines_ref  a reference to an array
#
sub file_to_array {
    my $filename = shift;
    my $lines_ref = shift;
    
    open(FILE, $filename) || die "can't open file '$filename': $!";
    @{$lines_ref} = <FILE>;
    close(FILE) || die "can't close file '$filename': $!";

    foreach my $a (@{$lines_ref}) {
        chomp($a);
    }
}


#
# unescape_string subroutine expands escape sequences found in the string and
# returns the expanded string. It also removes possible single or double quotes
# around the value.
#    Parameters:
#       value   a string
#    Return value:
#       a string with expanded escape sequences
# 
sub unescape_string {
    my $value = shift;
    my $result = '';
    my $offset = 0;

    # remove quotes around the value if they exist
    if (length($value) >= 2) {
        if ((substr($value, 0, 1) eq "'" && substr($value, -1, 1) eq "'")
            || (substr($value, 0, 1) eq '"' && substr($value, -1, 1) eq '"')) {
            $value = substr($value, 1, -1);
        }
    }
    
    # expand escape sequences
    while ($offset < length($value)) {
        my $pos = index($value, "\\", $offset);
        if ($pos < 0) {
            $pos = length($value);
            $result = $result . substr($value, $offset, $pos - $offset);
            $offset = $pos;
        } else {
            my $replacement = substr($value, $pos, 2);
            my $escape_code = substr($value, $pos + 1, 1);
            if (exists $option_value_escapes{$escape_code}) {
                $replacement = $option_value_escapes{$escape_code};
            }
            $result = $result 
                . substr($value, $offset, $pos - $offset)
                . $replacement;
            $offset = $pos + 2;
        }
    }

    return $result;
}


#
# read_config_file subroutine reads MySQL options file and
# returns the options in a hash containing one hash per group.
#    Parameters:
#       filename    name of a MySQL options file
#       groups_ref  a reference to hash variable where the read
#                   options are returned
#
sub read_config_file {
    #my $filename = shift;
    my $groups_ref = shift;
    my @lines ;
    my $i;
    my $group;
    my $group_hash_ref;

    my $cmdline = '';
    my $options = '';


    if ($option_apply_log) {
        $options = $options .
            " --defaults-file=\"${backup_dir}/backup-my.cnf\" ";
    } elsif ($option_defaults_file) {
        $options = $options . " --defaults-file=\"$option_defaults_file\" ";
    }

    if ($option_defaults_extra_file) {
        $options = $options . " --defaults-extra-file=\"$option_defaults_extra_file\" ";
    }

    if ($option_defaults_group) {
        $options = $options . " --defaults-group=\"$option_defaults_group\" ";
    }

    $options = $options . "--print-param";


    # read file to an array, one line per element
    #file_to_array($filename, \@lines);
    $cmdline = "$option_ibbackup_binary $options";
    @lines = `$cmdline`;

    # classify lines and save option values
    $group = 'default';
    $group_hash_ref = {}; 
    ${$groups_ref}{$group} = $group_hash_ref;
    # this pattern described an option value which may be
    # quoted with single or double quotes. This pattern
    # does not work by its own. It assumes that the first
    # opening parenthesis in this string is the second opening
    # parenthesis in the full pattern. 
    my $value_pattern = q/((["'])([^\\\4]|(\\[^\4]))*\4)|([^\s]+)/;
    for ($i = 0; $i < @lines; $i++) {
      SWITCH: for ($lines[$i]) {
          # comment
          /^\s*(#|;)/
             && do { last; };

          # group      
          /^\s*\[(.*)\]/ 
                && do { 
                    $group = $1; 
                    if (!exists ${$groups_ref}{$group}) {
                        $group_hash_ref = {}; 
                        ${$groups_ref}{$group} = $group_hash_ref;
                    } else {
                        $group_hash_ref = ${$groups_ref}{$group};
                    }
                    last; 
                };

          # option
          /^\s*([^\s=]+)\s*(#.*)?$/
              && do { 
                  ${$group_hash_ref}{$1} = '';
                  last; 
              };

          # set-variable = option = value
          /^\s*set-variable\s*=\s*([^\s=]+)\s*=\s*($value_pattern)\s*(#.*)?$/
              && do { ${$group_hash_ref}{$1} = unescape_string($2); last; };

          # option = value
          /^\s*([^\s=]+)\s*=\s*($value_pattern)\s*(#.*)?$/
              && do { ${$group_hash_ref}{$1} = unescape_string($2); last; };

          # empty line
          /^\s*$/
              && do { last; };

          # unknown
          print("$prefix: Warning: Ignored unrecognized line ",
                $i + 1,
                " in options : '${lines[$i]}'\n"
                );
      }
   }
}


# has_option_in_config return true if the configuration file defines an option
# with the given name.
#
#    Parameters:
#       option_name  name of the option
#    Return value:
#       true if option exists, otherwise false
#
sub has_option_in_config {
   my $option_name = shift;
   my $group_hash_ref;

   if (!exists $config{$option_defaults_group}) {
       return 0;
    }

    $group_hash_ref = $config{$option_defaults_group};

    return exists ${$group_hash_ref}{$option_name};
}


# has_option returns 1 if an option with the given name exists in either
# defaults file file as reported by 'xtrabackup --print-param' or (in backup
# mode) SHOW VARIABLES. Otherwise returns 0.
#
#    Parameters:
#       option_name  name of the option
#    Return value:
#       true if option exists, otherwise false
#
sub has_option {
    my $option_name = shift;

    if (has_option_in_config($option_name)) {
        return 1;
    }

    if ($option_backup) {
        if (!defined($mysql{vars})) {
            get_mysql_vars(\%mysql);
        }

        return defined($mysql{vars}->{$option_name});
    }

    return 0;
}


# get_option returns the value of an option with the given name as defined
# either in defaults file as reported by 'xtrabackup --print-param' or (in
# backup mode) SHOW VARIABLES.
#
# This subroutine aborts with an error if the option is not defined.
#
#  Parameters:
#       option_name  name of the option
#    Return value:
#       option value as a string
#
sub get_option {
    my $option_name = shift;
    my $group_hash_ref;

    if (!$option_backup) {
        if (!exists $config{$option_defaults_group}) {
            # no group
            die "no '$option_defaults_group' group in server configuration " .
                "file '$option_defaults_file'";
        }

        $group_hash_ref = $config{$option_defaults_group};
        if (!exists ${$group_hash_ref}{$option_name}) {
            # no option
            die "no '$option_name' option in group '$option_defaults_group' " .
                "in server configuration file '$option_defaults_file'";
        }

        return ${$group_hash_ref}{$option_name};
    }

    if (!defined($mysql{vars})) {
        get_mysql_vars(\%mysql);
    }

    if (!defined($mysql{vars}->{$option_name})) {
        die "no '$option_name' option in SHOW VARIABLES";
    }

    return $mysql{vars}->{$option_name}->{Value};
}

#
# Identical to get_option, except that the second argument is returned on error,
# i.e. if the option is not defined.
#
sub get_option_safe {
    my $option_name = shift;
    my $fallback_value = shift;

    if (has_option($option_name)) {
        return get_option($option_name);
    }

    return $fallback_value;
}

# get_table_name subroutine returns table name of specified file.
#    Parameters:
#       $_[0]        table path
#    Return value:
#       1 table name
#
sub get_table_name {
   my $table;

   $table = get_table_name_with_part_suffix($_[0]);
   # trim partition suffix
   $table = (split('#P#', $table))[0];

   return $table;
}

# Like get_table_name(), but keeps the partition suffix if present
sub get_table_name_with_part_suffix {
   my $table_path = shift;
   my $filename;
   my $table;

   # get the last component in the table pathname 
   $filename = (reverse(split(/\//, $table_path)))[0];
   # get name of the table by removing file suffix
   $table = (split(/\./, $filename))[0];

   return $table;
}

# check_if_required subroutine returns 1 if the specified database and
# table needs to be backed up.
#    Parameters:
#       $_[0]        name of database to be checked 
#       $_[1]        full path of table file (This argument is optional)
#    Return value:
#       1 if backup should be done and 0 if not
#
sub check_if_required {
   my ( $db, $table_path ) = @_;
   my $db_count  = scalar keys %databases_list;
   my $tbl_count = scalar keys %table_list;
   my $table;
   my $table_part;

   if ( $db_count == 0 && $tbl_count == 0 ) {
      # No databases defined with --databases option, include all databases,
      # and no tables defined with --tables-file option, include all tables.
       return 1;
   }
   else {
      if ( $table_path ) {
         $table_part = get_table_name_with_part_suffix($table_path);
         $table = get_table_name($table_path);
      }
   }

   # Filter for --databases.
   if ( $db_count ) {
      if (defined $databases_list{$db}) {
         if (defined $table_path) {
            my $db_hash = $databases_list{$db};
            $db_count = keys %$db_hash;
            if ($db_count > 0 &&
                !defined $databases_list{$db}->{$table_part} &&
                !defined $databases_list{$db}->{$table}) {
               # --databases option specified, but table is not included
               return 0;
            }
         }
         # include this database and table
         return 1;
      }
      else {
         # --databases option given, but database is not included
         return 0;
      }
   }

   # Filter for --tables-file.
   if ( $tbl_count ) {
      return 0 unless exists $table_list{$db};
      return 0 if $table && !$table_list{$db}->{$table_part} &&
          !$table_list{$db}->{$table};
   }

   return 1;  # backup the table
}


# parse_databases_option_value subroutine parses the value of 
# --databases option. If the option value begins with a slash
# it is considered a pathname and the option value is read
# from the file.
# 
# This subroutine sets the global "databases_list" variable.
#
sub parse_databases_option_value {
    my $item;

    if ($option_databases =~ /^\//) {
        # the value of the --databases option begins with a slash,
        # the option value is pathname of the file containing
        # list of databases
        if (! -f $option_databases) {
            die "can't find file '$option_databases'";
        }

        # read from file the value of --databases option
        my @lines;
    	file_to_array($option_databases, \@lines);
	$option_databases = join(" ", @lines);
    }

    # mark each database or database.table definition in the
    # global databases_list.
    foreach $item (split(/\s/, $option_databases)) {
        my $db = "";
        my $table = "";
        my %hash;

        if ($item eq "") {
            # ignore empty strings
            next;
        }

        # get database and table names
        if ($item =~ /(\S*)\.(\S*)/) {
            # item is of the form DATABASE.TABLE
            $db = $1;
            $table = $2;
        } else {
            # item is database name, table is undefined
            $db = $item;
        }

        if (! defined $databases_list{$db}) {
            # create empty hash for the database
            $databases_list{$db} = \%hash;
        }
        if ($table ne "") {
            # add mapping table --> 1 to the database hash
            my $h = $databases_list{$db};
            $h->{$table} = 1;
        }
    }
}

# Parse the --tables-file file to determine which InnoDB tables
# are backedup up.  Only backedup tables have their .frm, etc.
# files copied.
sub parse_tables_file_option_value {
   my ( $filename ) = @_;

   return unless $filename;

   open my $fh, '<', $filename;
   if ( $fh ) {
      while ( my $line = <$fh> ) {
         chomp $line;
         my ( $db, $tbl ) = $line =~ m/\s*([^\.]+)\.([^\.]+)\s*/;
         if ( $db && $tbl ) {
            $table_list{$db}->{$tbl} = 1;
         }
         else {
            warn "$prefix Invalid line in $filename: $line";
         }
      }
   }
   else {
      warn "$prefix Cannot read --tables-file $filename: $OS_ERROR";
   }

   return;
}

sub escape_path {
  my $str = shift;
  if ($win eq 1) {
    $str =~ s/\//\\/g;
    $str =~ s/\\\\/\\/g;
    }
  else{
    $str =~ s/\/\//\//g;
    }
  return $str;

}

sub check_server_version {
    my $var_version = '';
    my $var_innodb_version = '';

    get_mysql_vars(\%mysql);

    $var_version = $mysql{vars}->{version}->{Value};
    $var_innodb_version = $mysql{vars}->{innodb_version}->{Value};

    # Check supported versions
    if(!(
         # MySQL/Percona Server/MariaDB 5.1 or MariaDB 5.2/5.3 with InnoDB
         # plugin
         (
          $var_version =~ m/5\.[123]\.\d/
          && defined($var_innodb_version)) ||
         # MySQL/Percona Server/MariaDB 5.5
         $var_version =~ m/5\.5\.\d/ ||
         # MySQL/Percona Server 5.6
         $var_version =~ m/5\.6\.\d/ ||
         # MariaDB 10.0 / 10.1
         $var_version =~ m/10\.[01]\.\d/
        )) {

        if ($var_version =~ m/5\.1\./ &&
            !defined($var_innodb_version)) {
            die "Built-in InnoDB in MySQL 5.1 is not supported in this " .
                "release. You can either use Percona XtraBackup 2.0, " .
                "or upgrade to InnoDB plugin.\n";
        } else {
            die "Unsupported server version: '$var_version' " .
                "Please report a bug at " .
                "https://bugs.launchpad.net/percona-xtrabackup\n";
        }
    }
}

# Wait until it's safe to backup a slave.  Returns immediately if
# the host isn't a slave.  Currently there's only one check:
# Slave_open_temp_tables has to be zero.  Dies on timeout.
sub wait_for_safe_slave {
   my $con = shift;

   my @lines;
   # whether host is detected as slave in safe slave backup mode
   my $host_is_slave = 0;

   $sql_thread_started = 0;
   get_mysql_slave_status($con);

   if (defined($con->{slave_status}->{Read_Master_Log_Pos}) and
       defined($con->{slave_status}->{Slave_SQL_Running})) {
         $host_is_slave = 1;
   }

   if ( !$host_is_slave ) {
      print STDERR "$prefix: Not checking slave open temp tables for --safe-slave-backup because host is not a slave\n";
      return;
   }

   if ($con->{slave_status}->{Slave_SQL_Running} =~ m/Yes/ ) {
         $sql_thread_started = 1;
   }

   if ($sql_thread_started) {
       mysql_query($con, 'STOP SLAVE SQL_THREAD');
   }

   my $open_temp_tables = get_slave_open_temp_tables($con);
   print STDERR "$prefix: Slave open temp tables: $open_temp_tables\n";

   return if $open_temp_tables == 0;

   my $sleep_time = 3;
   my $n_attempts = int($option_safe_slave_backup_timeout / $sleep_time) || 1;
   while ( $n_attempts-- ) {
      print STDERR "$prefix: Starting slave SQL thread, waiting $sleep_time seconds, then checking Slave_open_temp_tables again ($n_attempts attempts remaining)...\n";
      
      mysql_query($con, 'START SLAVE SQL_THREAD');
      sleep $sleep_time;
      mysql_query($con, 'STOP SLAVE SQL_THREAD');

      $open_temp_tables = get_slave_open_temp_tables($con);
      print STDERR "$prefix: Slave open temp tables: $open_temp_tables\n";
      if ( !$open_temp_tables ) {
         print STDERR "$prefix: Slave is safe to backup\n";
         return;
      }
   } 

   # Restart the slave if it was running at start
   if ($sql_thread_started) {
       print STDERR "Restarting slave SQL thread.\n";
       mysql_query($con, 'START SLAVE SQL_THREAD');
   }

   die "Slave_open_temp_tables did not become zero after $option_safe_slave_backup_timeout seconds";
}

sub get_slave_open_temp_tables {
    my $con = shift;

    get_mysql_status($con);

    if (!defined($con->{status}->{Slave_open_temp_tables})) {
        die "Failed to get Slave_open_temp_tables from SHOW STATUS"
    }
    if (!defined($con->{status}->{Slave_open_temp_tables}->{Value})) {
        die "SHOW STATUS LIKE 'slave_open_temp_tables' did not return anything"
    }

   return $con->{status}->{Slave_open_temp_tables}->{Value};
}

#
# Streams a file into the backup set,
# handles any encryption.
# Should not be called directly, only meant to be called from backup_file.
#
sub backup_file_via_stream {
  my $src_path = shift;
  my $src_file = shift;
  my $ret = 0;

  my $filepos = rindex($src_file, '/');
  my $file_name = '';
  my $rel_path = '';

  if ($filepos >= 0) {
    $file_name = substr($src_file, $filepos + 1);
    $rel_path = substr($src_file, 0, $filepos);
  } else {
    $file_name = $src_file;
    $rel_path = '.';
  }
  $file_name=~s/([\$\\\" ])/\\$1/g;
  if ($encrypt_cmd) {
    $ret = system("cd $src_path; $stream_cmd $rel_path/$file_name | $encrypt_cmd") >> 8;
  } else {
    $ret = system("cd $src_path; $stream_cmd $rel_path/$file_name") >> 8;
  }

  if ($ret == 1 && $option_stream eq 'tar') {
    print STDERR "$prefix If you use GNU tar, this warning can be ignored.\n";
  # Check for non-zero exit code
  } elsif ($ret != 0) {
    if ($encrypt_cmd) {
      print STDERR "$prefix '$stream_cmd | $encrypt_cmd' returned with exit code $ret.\n";
    } else {
      print STDERR "$prefix '$stream_cmd' returned with exit code $ret.\n";
    }
    # Only treat as fatal cases where the file exists
    if ( -e "$src_path/$src_file" ) {
      die "Failed to stream '$src_path/$src_file': $ret";
    } else {
      print STDERR "$prefix Ignoring nonexistent file '$src_path/$src_file'.\n";
    }
  }
}

#
# Copies a file into the backup set,
# handles any encryption.
# Should not be called directly, only meant to be called from backup_file.
#
sub backup_file_via_copy {
  my $src_path = shift;
  my $src_file = shift;
  my $dst_file = shift;
  my $ret = 0;

  # Copy the file - If we get an error and the file actually exists, die with error msg
  my $src_file_esc = escape_path("$src_path/$src_file");
  my $dst_file_esc = escape_path("$dst_file");
  if ($encrypt_cmd) {
    $dst_file_esc = $dst_file_esc . ".xbcrypt";
    $ret = system("$encrypt_cmd -i \"$src_file_esc\" -o \"$dst_file_esc\"");
    if ($ret != 0) {
      die "Failed to copy and encrypt file '$src_file': $ret";
    }
  } elsif ( -e "$src_file_esc" ) {
    $ret = system("$CP_CMD \"$src_file_esc\" \"$dst_file_esc\"");
    if ($ret != 0) {
       die "Failed to copy file '$src_file': $ret";
     }
  }
}

#
# Copies or streams a file into the backup set,
# handles any streaming and/or encryption.
# All files to be placed into the backup set should only be put there
# through this call, write_backup_to_file ot xtrabackup itself. If any
# other method is used, files may not be properly formatted
# (stream, encryption, etc).
#
sub backup_file {
  my $src_path = shift;
  my $src_file = shift;
  my $dst_file = shift;

  if ($option_stream) {
    backup_file_via_stream($src_path, $src_file);
  } else {
    backup_file_via_copy($src_path, $src_file, $dst_file);
  }
}

#
# Writes data directly into a file within the root of the backup set,
# handles any streaming and/or encryption.
#
sub write_to_backup_file {
  my $file_name = shift;
  my $write_data = shift;

  if ($option_stream) {
    my $filepos = rindex($file_name, '/');
    my $dst_file = substr($file_name, $filepos + 1);
    my $dst_path = substr($file_name, 0, $filepos + 1);
 
    open XTRABACKUP_FH, ">", "$option_tmpdir/$dst_file"
      or die "Cannot open file $option_tmpdir/$dst_file: $!\n";
    print XTRABACKUP_FH $write_data;
    close XTRABACKUP_FH;
    backup_file($option_tmpdir, $dst_file, $file_name);
    unlink "$option_tmpdir/$dst_file";
  } else {
    open XTRABACKUP_FH, ">", "$file_name"
      or die "Cannot open file $file_name: $!\n";
    print XTRABACKUP_FH $write_data;
    close XTRABACKUP_FH;
  }
}

#
# Query the server to find out what backup capabilities it supports.
#
sub detect_mysql_capabilities_for_backup {
    my $con = shift;

    if ($option_incremental) {
        $have_changed_page_bitmaps =
            mysql_query($con, "SELECT COUNT(*) FROM " .
                        "INFORMATION_SCHEMA.PLUGINS " .
                        "WHERE PLUGIN_NAME LIKE 'INNODB_CHANGED_PAGES'");
    }

    if (!defined($con->{vars})) {
        get_mysql_vars($con);
    }

    if (!defined($con->{slave_status})) {
        get_mysql_slave_status($con);
    }

    $have_backup_locks = defined($con->{vars}->{have_backup_locks});

    $have_galera_enabled = defined($con->{vars}->{wsrep_on});

    if ($option_galera_info && !$have_galera_enabled) {
        my $now = current_time();

        print STDERR "$now  $prefix --galera-info is specified on the command " .
            "line, but the server does not support Galera replication. " .
            "Ignoring the option.";

        $option_galera_info = 0;
    }

    if ($con->{vars}->{version}->{Value} =~ m/5\.[123]\.\d/) {
        my $now = current_time();

        print STDERR "\n$now  $prefix Warning: FLUSH ENGINE LOGS " .
            "is not supported " .
            "by the server. Data may be inconsistent with " .
            "binary log coordinates!\n";

        $have_flush_engine_logs = 0;
    } else {
        $have_flush_engine_logs = 1;
    }

    if (defined($con->{vars}->{slave_parallel_workers}) and
        ($con->{vars}->{slave_parallel_workers}->{Value} > 0)) {
        $have_multi_threaded_slave = 1;
    }

    my $gtid_executed = $con->{slave_status}->{Executed_Gtid_Set};
    my $gtid_slave_pos = $con->{vars}->{gtid_slave_pos};

    if ((defined($gtid_executed) and $gtid_executed ne '') or
        (defined($gtid_slave_pos) and $gtid_slave_pos ne '')) {
        $have_gtid_slave = 1;
    }

    $have_lock_wait_timeout = defined($con->{vars}->{lock_wait_timeout});
}

#
# Writes xtrabackup_info file and if backup_history is enable creates
# PERCONA_SCHEMA.xtrabackup_history and writes a new history record to the
# table containing all the history info particular to the just completed
# backup.
#
sub write_xtrabackup_info()
{
  my $con = shift;
  my $sth;
  my $uuid;
  my $tmp;
  my $file_content;

  eval {
    $sth = $con->{dbh}->prepare("insert into PERCONA_SCHEMA.xtrabackup_history(".
                  "uuid, name, tool_name, tool_command, tool_version, ".
                  "ibbackup_version, server_version, start_time, end_time, ".
                  "lock_time, binlog_pos, innodb_from_lsn, innodb_to_lsn, ".
                  "partial, incremental, format, compact, compressed, ".
                  "encrypted) ".
                  "values(?,?,?,?,?,?,?,from_unixtime(?),from_unixtime(?),?,?,".
                  "?,?,?,?,?,?,?,?)");
  } || die("Eror while preparing history record\n");


  # uuid
  # we use the mysql UUID() function to avoid platform dependencies with uuidgen
  # and Data::UUID. We select uuid here individually here so that it can be
  # reported to stderr after successful history record insertion
  eval {
    $uuid = $con->{dbh}->selectrow_hashref("SELECT UUID() AS uuid")->{uuid};
  } || die("Error while attempting to create UUID for history record.\n");
  $sth->bind_param(1, $uuid);
  $file_content = "uuid = $uuid";

  # name
  if ($option_history) {
    $sth->bind_param(2, $option_history);
    $file_content .= "\nname = $option_history";
  } else {
    $sth->bind_param(2, undef);
    $file_content .= "\nname = ";
  }

  # tool_name
  $tmp = basename($0);
  $sth->bind_param(3, $tmp);
  $file_content .= "\ntool_name = $tmp";

  # tool_command
  # scrub --password and --encrypt-key from tool_command
  $tmp = $history_tool_command;
  $tmp =~ s/--password=[^ ]+/--password=.../g;
  $tmp =~ s/--encrypt-key=[^ ]+/--encrypt-key=.../g;
  $sth->bind_param(4, $tmp);
  $file_content .= "\ntool_command = $tmp";

  # tool_version
  $sth->bind_param(5, $innobackup_version);
  $file_content .= "\ntool_version = $innobackup_version";

  # ibbackup_version
  $tmp = "$option_ibbackup_binary -v 2>&1";
  $tmp = qx($tmp);
  chomp($tmp);
  if (length $tmp) {
    $sth->bind_param(6, $tmp);
    $file_content .= "\nibbackup_version = $tmp";
  } else {
    $sth->bind_param(6, undef);
    $file_content .= "\nibbackup_version = ";
  }

  # server_version
  $tmp =
    $con->{dbh}->selectrow_hashref("SELECT VERSION() as version")->{version};
  $sth->bind_param(7, $tmp);
  $file_content .= "\nserver_version = $tmp";

  # start_time
  $sth->bind_param(8, $history_start_time);
  $file_content .= "\nstart_time = " . strftime("%Y-%m-%d %H:%M:%S", localtime($history_start_time));

  # end_time
  $tmp = time();
  $sth->bind_param(9, $tmp);
  $file_content .= "\nend_time = " . strftime("%Y-%m-%d %H:%M:%S", localtime($tmp));

  # lock_time
  $sth->bind_param(10, $history_lock_time);
  $file_content .= "\nlock_time = $history_lock_time";

  # binlog_pos
  if ($mysql_binlog_position) {
    $sth->bind_param(11, $mysql_binlog_position);
    $file_content .= "\nbinlog_pos = $mysql_binlog_position";
  } else {
    $sth->bind_param(11, undef);
    $file_content .= "\nbinlog_pos = ";
  }

  # innodb_from_lsn
  # grab the from_lsn from the extra checkpoints file, parse and delete it
  # if necessary. This could generate an error if the checkpoints file
  # isn't 100% correct in the format.
  $tmp = "cat $option_extra_lsndir/xtrabackup_checkpoints | grep from_lsn";
  $tmp = qx($tmp);
  chomp($tmp);
  substr($tmp, 0, 11, ""); # strip "from_lsn = "
  if (length $tmp) {
    $sth->bind_param(12, $tmp);
    $file_content .= "\ninnodb_from_lsn = $tmp";
  } else {
    print STDERR "WARNING : Unable to obtain from_lsn from " .
                 "$option_extra_lsndir/xtrabackup_checkpoints, " .
                 "writing NULL to history record.\n";
    $sth->bind_param(12, undef);
    $file_content .= "\ninnodb_from_lsn = ";
  }

  # innodb_to_lsn
  # grab the to_lsn from the extra checkpoints file, parse and delete it
  # if necessary. This could generate an error if the checkpoints file
  # isn't 100% correct in the format.
  $tmp = "cat $option_extra_lsndir/xtrabackup_checkpoints | grep to_lsn";
  $tmp = qx($tmp);
  chomp($tmp);
  substr($tmp, 0, 9, ""); # strip "to_lsn = "
  if (length $tmp) {
    $sth->bind_param(13, $tmp);
    $file_content .= "\ninnodb_to_lsn = $tmp";
  } else {
    print STDERR "WARNING : Unable to obtain to_lsn from " .
                 "$option_extra_lsndir/xtrabackup_checkpoints, " .
                 "writing NULL to history record.\n";
    $sth->bind_param(13, undef);
    $file_content .= "\ninnodb_to_lsn = ";
  }

  # we're finished with the checkpoints file, delete it if it was a temp
  # only for the history
  if ($history_delete_checkpoints > 0) {
    unlink("$option_extra_lsndir/xtrabackup_checkpoints");
  }

  # partial (Y | N)
  if ($option_include || $option_databases || $option_tables_file || $option_export) {
    $sth->bind_param(14, "Y");
    $file_content .= "\npartial = Y";
  } else {
    $sth->bind_param(14, "N");
    $file_content .= "\npartial = N";
  }

  # incremental (Y | N)
  if ($option_incremental) {
    $sth->bind_param(15, "Y");
    $file_content .= "\nincremental = Y";
  } else {
    $sth->bind_param(15, "N");
    $file_content .= "\nincremental = N";
  }

  # format (file | tar | xbsream)
  if ($option_stream eq 'tar') {
    $sth->bind_param(16, "tar");
    $file_content .= "\nformat = tar";
  } elsif ($option_stream eq 'xbstream') {
    $sth->bind_param(16, "xbstream");
    $file_content .= "\nformat = xbstream";
  } else {
    $sth->bind_param(16, "file");
    $file_content .= "\nformat = file";
  }

  # compact (Y | N)
  if ($option_compact) {
    $sth->bind_param(17, "Y");
    $file_content .= "\ncompact = Y";
  } else {
    $sth->bind_param(17, "N");
    $file_content .= "\ncompact = N";
  }

  # compressed (Y | N)
  if ($option_compress) {
    $sth->bind_param(18, "Y");
    $file_content .= "\ncompressed = Y";
  } else {
    $sth->bind_param(18, "N");
    $file_content .= "\ncompressed = N";
  }

  # encrypted (Y | N)
  if ($option_encrypt) {
    $sth->bind_param(19, "Y");
    $file_content .= "\nencrypted = Y";
  } else {
    $sth->bind_param(19, "N");
    $file_content .= "\nencrypted = N";
  }

  # create the backup history file
  write_to_backup_file("$backup_history", "$file_content");

  if (!defined($option_history)) {
    return;
  }

  mysql_query($con, "CREATE DATABASE IF NOT EXISTS PERCONA_SCHEMA");
  mysql_query($con, "CREATE TABLE IF NOT EXISTS PERCONA_SCHEMA.xtrabackup_history(".
                    "uuid VARCHAR(40) NOT NULL PRIMARY KEY,".
                    "name VARCHAR(255) DEFAULT NULL,".
                    "tool_name VARCHAR(255) DEFAULT NULL,".
                    "tool_command TEXT DEFAULT NULL,".
                    "tool_version VARCHAR(255) DEFAULT NULL,".
                    "ibbackup_version VARCHAR(255) DEFAULT NULL,".
                    "server_version VARCHAR(255) DEFAULT NULL,".
                    "start_time TIMESTAMP NULL DEFAULT NULL,".
                    "end_time TIMESTAMP NULL DEFAULT NULL,".
                    "lock_time BIGINT UNSIGNED DEFAULT NULL,".
                    "binlog_pos VARCHAR(128) DEFAULT NULL,".
                    "innodb_from_lsn BIGINT UNSIGNED DEFAULT NULL,".
                    "innodb_to_lsn BIGINT UNSIGNED DEFAULT NULL,".
                    "partial ENUM('Y', 'N') DEFAULT NULL,".
                    "incremental ENUM('Y', 'N') DEFAULT NULL,".
                    "format ENUM('file', 'tar', 'xbstream') DEFAULT NULL,".
                    "compact ENUM('Y', 'N') DEFAULT NULL,".
                    "compressed ENUM('Y', 'N') DEFAULT NULL,".
                    "encrypted ENUM('Y', 'N') DEFAULT NULL".
                    ") CHARACTER SET utf8 ENGINE=innodb");

  eval {
    $sth->execute;
  } || die("Error while attempting to insert history record.\n");

  print STDERR "$prefix Backup history record uuid $uuid successfully written\n";
}


=pod

=head1 NAME

innobackupex - Non-blocking backup tool for InnoDB, XtraDB and HailDB databases

=head1 SYNOPOSIS

innobackupex [--compress] [--compress-threads=NUMBER-OF-THREADS] [--compress-chunk-size=CHUNK-SIZE]
             [--encrypt=ENCRYPTION-ALGORITHM] [--encrypt-threads=NUMBER-OF-THREADS] [--encrypt-chunk-size=CHUNK-SIZE]
             [--encrypt-key=LITERAL-ENCRYPTION-KEY] | [--encryption-key-file=MY.KEY]
             [--include=REGEXP] [--user=NAME]
             [--password=WORD] [--port=PORT] [--socket=SOCKET]
             [--no-timestamp] [--ibbackup=IBBACKUP-BINARY]
             [--slave-info] [--galera-info] [--stream=tar|xbstream]
             [--defaults-file=MY.CNF] [--defaults-group=GROUP-NAME]
             [--databases=LIST] [--no-lock] 
             [--tmpdir=DIRECTORY] [--tables-file=FILE]
             [--history=NAME]
             [--incremental] [--incremental-basedir]
             [--incremental-dir] [--incremental-force-scan] [--incremental-lsn]
             [--incremental-history-name=NAME] [--incremental-history-uuid=UUID]
             [--close-files] [--compact]     
             BACKUP-ROOT-DIR

innobackupex --apply-log [--use-memory=B]
             [--defaults-file=MY.CNF]
             [--export] [--redo-only] [--ibbackup=IBBACKUP-BINARY]
             BACKUP-DIR

innobackupex --copy-back [--defaults-file=MY.CNF] [--defaults-group=GROUP-NAME] BACKUP-DIR

innobackupex --move-back [--defaults-file=MY.CNF] [--defaults-group=GROUP-NAME] BACKUP-DIR

innobackupex [--decompress] [--decrypt=ENCRYPTION-ALGORITHM]
             [--encrypt-key=LITERAL-ENCRYPTION-KEY] | [--encryption-key-file=MY.KEY]
             [--parallel=NUMBER-OF-FORKS] BACKUP-DIR

=head1 DESCRIPTION

The first command line above makes a hot backup of a MySQL database.
By default it creates a backup directory (named by the current date
and time) in the given backup root directory.  With the --no-timestamp
option it does not create a time-stamped backup directory, but it puts
the backup in the given directory (which must not exist).  This
command makes a complete backup of all MyISAM and InnoDB tables and
indexes in all databases or in all of the databases specified with the
--databases option.  The created backup contains .frm, .MRG, .MYD,
.MYI, .MAD, .MAI, .TRG, .TRN, .ARM, .ARZ, .CSM, CSV, .opt, .par, and
InnoDB data and log files.  The MY.CNF options file defines the
location of the database.  This command connects to the MySQL server
using the mysql client program, and runs xtrabackup as a child
process.

The --apply-log command prepares a backup for starting a MySQL
server on the backup. This command recovers InnoDB data files as specified
in BACKUP-DIR/backup-my.cnf using BACKUP-DIR/xtrabackup_logfile,
and creates new InnoDB log files as specified in BACKUP-DIR/backup-my.cnf.
The BACKUP-DIR should be the path to a backup directory created by
xtrabackup. This command runs xtrabackup as a child process, but it does not 
connect to the database server.

The --copy-back command copies data, index, and log files
from the backup directory back to their original locations.
The MY.CNF options file defines the original location of the database.
The BACKUP-DIR is the path to a backup directory created by xtrabackup.

The --move-back command is similar to --copy-back with the only difference that
it moves files to their original locations rather than copies them. As this
option removes backup files, it must be used with caution. It may be useful in
cases when there is not enough free disk space to copy files.

The --decompress --decrypt command will decrypt and/or decompress a backup made
with the --compress and/or --encrypt options. When decrypting, the encryption
algorithm and key used when the backup was taken MUST be provided via the
specified options. --decrypt and --decompress may be used together at the same
time to completely normalize a previously compressed and encrypted backup. The
--parallel option will allow multiple files to be decrypted and/or decompressed
simultaneously. In order to decompress, the qpress utility MUST be installed
and accessable within the path. This process will remove the original
compressed/encrypted files and leave the results in the same location.

On success the exit code innobackupex is 0. A non-zero exit code 
indicates an error.


=head1 OPTIONS

=over

=item --apply-log

Prepare a backup in BACKUP-DIR by applying the transaction log file named "xtrabackup_logfile" located in the same directory. Also, create new transaction logs. The InnoDB configuration is read from the file "backup-my.cnf".

=item --close-files

Do not keep files opened. This option is passed directly to xtrabackup. Use at your own risk.

=item --compact

Create a compact backup with all secondary index pages omitted. This option is passed directly to xtrabackup. See xtrabackup documentation for details.

=item --compress

This option instructs xtrabackup to compress backup copies of InnoDB
data files. It is passed directly to the xtrabackup child process. Try
'xtrabackup --help' for more details.

=item --compress-threads

This option specifies the number of worker threads that will be used
for parallel compression. It is passed directly to the xtrabackup
child process. Try 'xtrabackup --help' for more details.

=item --compress-chunk-size

This option specifies the size of the internal working buffer for each
compression thread, measured in bytes. It is passed directly to the
xtrabackup child process. Try 'xtrabackup --help' for more details.

=item --copy-back

Copy all the files in a previously made backup from the backup directory to their original locations.

=item --databases=LIST

This option specifies the list of databases that innobackupex should back up. The option accepts a string argument or path to file that contains the list of databases to back up. The list is of the form "databasename1[.table_name1] databasename2[.table_name2] . . .". If this option is not specified, all databases containing MyISAM and InnoDB tables will be backed up.  Please make sure that --databases contains all of the InnoDB databases and tables, so that all of the innodb.frm files are also backed up. In case the list is very long, this can be specified in a file, and the full path of the file can be specified instead of the list. (See option --tables-file.)

=item --decompress

Decompresses all files with the .qp extension in a backup previously made with the --compress option.

=item --decrypt=ENCRYPTION-ALGORITHM

Decrypts all files with the .xbcrypt extension in a backup previously made with --encrypt option.

=item --debug-sleep-before-unlock=SECONDS

This is a debug-only option used by the XtraBackup test suite.

=item --defaults-file=[MY.CNF]

This option specifies what file to read the default MySQL options from.  The option accepts a string argument. It is also passed directly to xtrabackup's --defaults-file option. See the xtrabackup documentation for details.

=item --defaults-group=GROUP-NAME

This option specifies the group name in my.cnf which should be used. This is needed for mysqld_multi deployments.

=item --defaults-extra-file=[MY.CNF]

This option specifies what extra file to read the default MySQL options from before the standard defaults-file.  The option accepts a string argument. It is also passed directly to xtrabackup's --defaults-extra-file option. See the xtrabackup documentation for details.

=item --encrypt=ENCRYPTION-ALGORITHM

This option instructs xtrabackup to encrypt backup copies of InnoDB data
files using the algorithm specified in the ENCRYPTION-ALGORITHM.
It is passed directly to the xtrabackup child process.
Try 'xtrabackup --help' for more details.

=item --encrypt-key=ENCRYPTION-KEY

This option instructs xtrabackup to use the given ENCRYPTION-KEY when using the --encrypt or --decrypt options.
During backup it is passed directly to the xtrabackup child process.
Try 'xtrabackup --help' for more details.

=item --encrypt-key-file=ENCRYPTION-KEY-FILE

This option instructs xtrabackup to use the encryption key stored in the given ENCRYPTION-KEY-FILE when using the --encrypt or --decrypt options.

Try 'xtrabackup --help' for more details.

=item --encrypt-threads

This option specifies the number of worker threads that will be used
for parallel encryption. It is passed directly to the xtrabackup
child process. Try 'xtrabackup --help' for more details.

=item --encrypt-chunk-size

This option specifies the size of the internal working buffer for each
encryption thread, measured in bytes. It is passed directly to the
xtrabackup child process. Try 'xtrabackup --help' for more details.

=item --export

This option is passed directly to xtrabackup's --export option. It enables exporting individual tables for import into another server. See the xtrabackup documentation for details.

=item --extra-lsndir=DIRECTORY

This option specifies the directory in which to save an extra copy of the "xtrabackup_checkpoints" file.  The option accepts a string argument. It is passed directly to xtrabackup's --extra-lsndir option. See the xtrabackup documentation for details.

==item --force-non-empty-directories 

This option, when specified, makes --copy-back or --move-back transfer files to non-empty directories. Note that no existing files will be overwritten. If --copy-back or --nove-back has to copy a file from the backup directory which already exists in the destination directory, it will still fail with an error.

=item --galera-info

This options creates the xtrabackup_galera_info file which contains the local node state at the time of the backup. Option should be used when performing the backup of Percona-XtraDB-Cluster. Has no effect when backup locks are used to create the backup.

=item --help

This option displays a help screen and exits.

=item --history=NAME

This option enables the tracking of backup history in the PERCONA_SCHEMA.xtrabackup_history table. An optional history series name may be specified that will be placed with the history record for the current backup being taken.

=item --host=HOST

This option specifies the host to use when connecting to the database server with TCP/IP.  The option accepts a string argument. It is passed to the mysql child process without alteration. See mysql --help for details.

=item --ibbackup=IBBACKUP-BINARY

This option specifies which xtrabackup binary should be used.  The option accepts a string argument. IBBACKUP-BINARY should be the command used to run XtraBackup. The option can be useful if the xtrabackup binary is not in your search path or working directory. If this option is not specified, innobackupex attempts to determine the binary to use automatically. By default, "xtrabackup" is the command used. However, when option --copy-back is specified, "xtrabackup_51" is the command used. And when option --apply-log is specified, the binary is used whose name is in the file "xtrabackup_binary" in the backup directory, if that file exists.

=item --include=REGEXP

This option is a regular expression to be matched against table names in databasename.tablename format. It is passed directly to xtrabackup's --tables option. See the xtrabackup documentation for details.

=item --incremental

This option tells xtrabackup to create an incremental backup, rather than a full one. It is passed to the xtrabackup child process. When this option is specified, either --incremental-lsn or --incremental-basedir can also be given. If neither option is given, option --incremental-basedir is passed to xtrabackup by default, set to the first timestamped backup directory in the backup base directory.

=item --incremental-basedir=DIRECTORY

This option specifies the directory containing the full backup that is the base dataset for the incremental backup.  The option accepts a string argument. It is used with the --incremental option.

=item --incremental-dir=DIRECTORY

This option specifies the directory where the incremental backup will be combined with the full backup to make a new full backup.  The option accepts a string argument. It is used with the --incremental option.

=item --incremental-history-name=NAME

This option specifies the name of the backup series stored in the PERCONA_SCHEMA.xtrabackup_history history record to base an incremental backup on. Xtrabackup will search the history table looking for the most recent (highest innodb_to_lsn), successful backup in the series and take the to_lsn value to use as the starting lsn for the incremental backup. This will be mutually exclusive with --incremental-history-uuid, --incremental-basedir and --incremental-lsn. If no valid lsn can be found (no series by that name, no successful backups by that name) xtrabackup will return with an error. It is used with the --incremental option.

=item --incremental-history-uuid=UUID

This option specifies the UUID of the specific history record stored in the PERCONA_SCHEMA.xtrabackup_history to base an incremental backup on. --incremental-history-name, --incremental-basedir and --incremental-lsn. If no valid lsn can be found (no success record with that uuid) xtrabackup will return with an error. It is used with the --incremental option.

=item --incremental-force-scan

This options tells xtrabackup to perform full scan of data files for taking an incremental backup even if full changed page bitmap data is available to enable the backup without the full scan.

=item --log-copy-interval

This option specifies time interval between checks done by log copying thread in milliseconds.

=item --incremental-lsn

This option specifies the log sequence number (LSN) to use for the incremental backup.  The option accepts a string argument. It is used with the --incremental option. It is used instead of specifying --incremental-basedir. For databases created by MySQL and Percona Server 5.0-series versions, specify the LSN as two 32-bit integers in high:low format. For databases created in 5.1 and later, specify the LSN as a single 64-bit integer.

=item --kill-long-queries-timeout=SECONDS

This option specifies the number of seconds innobackupex waits between starting FLUSH TABLES WITH READ LOCK and killing those queries that block it. Default is 0 seconds, which means innobackupex will not attempt to kill any queries.

=item --kill-long-query-type=all|update

This option specifies which types of queries should be killed to unblock the global lock. Default is "all".

=item --lock-wait-timeout=SECONDS

This option specifies time in seconds that innobackupex should wait for queries that would block FTWRL before running it. If there are still such queries when the timeout expires, innobackupex terminates with an error.
Default is 0, in which case innobackupex does not wait for queries to complete and starts FTWRL immediately.

=item --lock-wait-threshold=SECONDS

This option specifies the query run time threshold which is used by innobackupex to detect long-running queries with a non-zero value of --lock-wait-timeout. FTWRL is not started until such long-running queries exist. This option has no effect if --lock-wait-timeout is 0. Default value is 60 seconds.

=item --lock-wait-query-type=all|update

This option specifies which types of queries are allowed to complete before innobackupex will issue the global lock. Default is all.

=item --move-back

Move all the files in a previously made backup from the backup directory to the actual datadir location. Use with caution, as it removes backup files.

=item --no-lock

Use this option to disable table lock with "FLUSH TABLES WITH READ LOCK". Use it only if ALL your tables are InnoDB and you DO NOT CARE about the binary log position of the backup. This option shouldn't be used if there are any DDL statements being executed or if any updates are happening on non-InnoDB tables (this includes the system MyISAM tables in the mysql database), otherwise it could lead to an inconsistent backup. If you are considering to use --no-lock because your backups are failing to acquire the lock, this could be because of incoming replication events preventing the lock from succeeding. Please try using --safe-slave-backup to momentarily stop the replication slave thread, this may help the backup to succeed and you then don't need to resort to using this option.

=item --no-timestamp

This option prevents creation of a time-stamped subdirectory of the BACKUP-ROOT-DIR given on the command line. When it is specified, the backup is done in BACKUP-ROOT-DIR instead.

=item --no-version-check

This option disables the version check which is enabled by the --version-check option.

=item --parallel=NUMBER-OF-THREADS

On backup, this option specifies the number of threads the xtrabackup child process should use to back up files concurrently.  The option accepts an integer argument. It is passed directly to xtrabackup's --parallel option. See the xtrabackup documentation for details.
 
On --decrypt or --decompress it specifies the number of parallel forks that should be used to process the backup files.

=item --password=WORD

This option specifies the password to use when connecting to the database. It accepts a string argument.  It is passed to the mysql child process without alteration. See mysql --help for details.

=item --port=PORT

This option specifies the port to use when connecting to the database server with TCP/IP.  The option accepts a string argument. It is passed to the mysql child process. It is passed to the mysql child process without alteration. See mysql --help for details.

=item --rebuild-indexes

This option only has effect when used together with the --apply-log option and is passed directly to xtrabackup. When used, makes xtrabackup rebuild all secondary indexes after applying the log. This option is normally used to prepare compact backups. See the XtraBackup manual for more information.

=item --rebuild-threads

This option only has effect when used together with the --apply-log and --rebuild-indexes option and is passed directly to xtrabackup. When used, xtrabackup processes tablespaces in parallel with the specified number of threads when rebuilding indexes. See the XtraBackup manual for more information.

=item --redo-only

This option should be used when preparing the base full backup and when merging all incrementals except the last one. This option is passed directly to xtrabackup's --apply-log-only option. This forces xtrabackup to skip the "rollback" phase and do a "redo" only. This is necessary if the backup will have incremental changes applied to it later. See the xtrabackup documentation for details. 

=item --rsync

Uses the rsync utility to optimize local file transfers. When this option is specified, innobackupex uses rsync to copy all non-InnoDB files instead of spawning a separate cp for each file, which can be much faster for servers with a large number of databases or tables.  This option cannot be used together with --stream.

=item --safe-slave-backup

Stop slave SQL thread and wait to start backup until Slave_open_temp_tables in "SHOW STATUS" is zero. If there are no open temporary tables, the backup will take place, otherwise the SQL thread will be started and stopped until there are no open temporary tables. The backup will fail if Slave_open_temp_tables does not become zero after --safe-slave-backup-timeout seconds. The slave SQL thread will be restarted when the backup finishes.

=item --safe-slave-backup-timeout

How many seconds --safe-slave-backup should wait for Slave_open_temp_tables to become zero. (default 300)

=item --slave-info

This option is useful when backing up a replication slave server. It prints the binary log position and name of the master server. It also writes this information to the "xtrabackup_slave_info" file as a "CHANGE MASTER" command. A new slave for this master can be set up by starting a slave server on this backup and issuing a "CHANGE MASTER" command with the binary log position saved in the "xtrabackup_slave_info" file.

=item --socket=SOCKET

This option specifies the socket to use when connecting to the local database server with a UNIX domain socket.  The option accepts a string argument. It is passed to the mysql child process without alteration. See mysql --help for details.

=item --stream=STREAMNAME

This option specifies the format in which to do the streamed backup.  The option accepts a string argument. The backup will be done to STDOUT in the specified format. Currently, the only supported formats are tar and xbstream. This option is passed directly to xtrabackup's --stream option.

=item --tables-file=FILE

This option specifies the file in which there are a list of names of the form database.  The option accepts a string argument.table, one per line. The option is passed directly to xtrabackup's --tables-file option.

=item --throttle=IOS

This option specifies a number of I/O operations (pairs of read+write) per second.  It accepts an integer argument.  It is passed directly to xtrabackup's --throttle option.

=item --tmpdir=DIRECTORY

This option specifies the location where a temporary file will be stored.  The option accepts a string argument. It should be used when --stream is specified. For these options, the transaction log will first be stored to a temporary file, before streaming. This option specifies the location where that temporary file will be stored. If the option is not specified, the default is to use the value of tmpdir read from the server configuration.

=item --use-memory=B

This option accepts a string argument that specifies the amount of memory in bytes for xtrabackup to use for crash recovery while preparing a backup. Multiples are supported providing the unit (e.g. 1MB, 1GB). It is used only with the option --apply-log. It is passed directly to xtrabackup's --use-memory option. See the xtrabackup documentation for details.

=item --user=NAME

This option specifies the MySQL username used when connecting to the server, if that's not the current user. The option accepts a string argument.  It is passed to the mysql child process without alteration. See mysql --help for details.

=item --version

This option displays the xtrabackup version and copyright notice and then exits.

=item --version-check

This option controls if the version check should be executed by innobackupex after connecting to the server on the backup stage. This option is enabled by default, disable with --no-version-check.

=back

=head1 BUGS

Bugs can be reported on Launchpad: https://bugs.launchpad.net/percona-xtrabackup/+filebug

=head1 COPYRIGHT

InnoDB Backup Utility Copyright 2003, 2009 Innobase Oy and Percona, Inc 2009-2012. All Rights Reserved.

This software is published under the GNU GENERAL PUBLIC LICENSE Version 2, June 1991.

=cut
