#!/bin/sh

# Postfix installation script. Run from the top-level Postfix source directory.
#
# Usage: sh INSTALL.sh [-non-interactive] name=value ...
#
# Non-interective mode uses settings from /etc/postfix/main.cf (or
# from /etc/postfix/install.cf when upgrading from a < 2002 release).

PATH=/bin:/usr/bin:/usr/sbin:/usr/etc:/sbin:/etc:/usr/contrib/bin:/usr/gnu/bin:/usr/ucb:/usr/bsd
umask 022

# Process command-line settings

for arg
do
    case $arg in
             *=*) IFS= eval $arg;;
-non-interactive) non_interactive=1;;
               *) echo Error: usage: $0 [-non-interactive] name=value ... 1>&2
		  exit 1;;
    esac
done

# Discourage old habits.

test -z "$non_interactive" -a ! -t 0 && {
    echo Error: for non-interactive installation, run: \"$0 -non-interactive\" 1>&2
    exit 1
}

test -z "$non_interactive" && cat <<EOF

Warning: this script replaces existing sendmail or Postfix programs.
Make backups if you want to be able to recover.

Before installing files, this script prompts you for some definitions.
Most definitions will be remembered, so you have to specify them
only once. All definitions have a reasonable default value.
EOF

# By now, shells must have functions. Ultrix users must use sh5 or lose.
# The following shell functions replace files/symlinks while minimizing
# the time that a file does not exist, and avoid copying over programs
# in order to not disturb running programs.

censored_ls() {
    ls "$@" | egrep -v '^\.|/\.|CVS|RCS|SCCS'
}

compare_or_replace() {
    (cmp $2 $3 >/dev/null 2>&1 && echo Skipping $3...) || {
	echo Updating $3...
	rm -f $tempdir/junk || exit 1
	cp $2 $tempdir/junk || exit 1
	chmod $1 $tempdir/junk || exit 1
	mv -f $tempdir/junk $3 || exit 1
	chmod $1 $3 || exit 1
    }
}

compare_or_symlink() {
    (cmp $1 $2 >/dev/null 2>&1 && echo Skipping $2...) || {
	echo Updating $2...
	rm -f $tempdir/junk || exit 1
	dest=`echo $1 | sed '
	    s;^'$install_root';;
	    s;/\./;/;g
	    s;//*;/;g
	    s;^/;;
	'`
	link=`echo $2 | sed '
	    s;^'$install_root';;
	    s;/\./;/;g
	    s;//*;/;g
	    s;^/;;
	    s;/[^/]*$;/;
	    s;[^/]*/;../;g
	    s;$;'$dest';
	'`
	ln -s $link $tempdir/junk || exit 1
	mv -f $tempdir/junk $2 || { 
	    echo Error: your mv command is unable to rename symlinks. 1>&2
	    echo If you run Linux, upgrade to GNU fileutils-4.0 or better, 1>&2
	    echo or choose a tempdir that is in the same file system as $2. 1>&2
	    exit 1
	}
    }
}

compare_or_move() {
    (cmp $2 $3 >/dev/null 2>&1 && echo Skipping $3...) || {
	echo Updating $3...
	mv -f $2 $3 || exit 1
	chmod $1 $3 || exit 1
    }
}

# How to supress newlines in echo

case `echo -n` in
"") n=-n; c=;;
 *) n=; c='\c';;
esac

# Prompts.

install_root_prompt="the prefix for installed file names. This is
useful only if you are building ready-to-install packages for other
machines."

tempdir_prompt="directory for scratch files while installing Postfix.
You must must have write permission in this directory."

config_directory_prompt="the directory with Postfix configuration
files. For security reasons this directory must be owned by the
super-user."

daemon_directory_prompt="the directory with Postfix daemon programs.
This directory should not be in the command search path of any
users."

command_directory_prompt="the directory with Postfix administrative
commands.  This directory should be in the command search path of
adminstrative users."

queue_directory_prompt="the directory with Postfix queues."

sendmail_path_prompt="the full pathname of the Postfix sendmail
command. This is the Sendmail-compatible mail posting interface."

newaliases_path_prompt="the full pathname of the Postfix newaliases
command.  This is the Sendmail-compatible command to build alias
databases."

mailq_path_prompt="the full pathname of the Postfix mailq command.
This is the Sendmail-compatible mail queue listing command."

mail_owner_prompt="the owner of the Postfix queue. Specify a user
account with numerical user ID and group ID values that are not
used by any other user accounts."

setgid_group_prompt="the group for mail submission and for queue
management commands.  Specify a group name with a numerical group
ID that is not shared with other accounts, not even with the Postfix
account."

manpage_path_prompt="where to install the Postfix on-line manual
pages."

# Default settings, just to get started.

: ${install_root=/}
: ${tempdir=`pwd`}
: ${config_directory=`bin/postconf -c conf -h -d config_directory`}

# Find out the location of configuration files.

test -z "$non_interactive" && for name in install_root tempdir config_directory
do
    while :
    do
	echo
	eval echo Please specify \$${name}_prompt | fmt
	eval echo \$n "$name: [\$$name]\  \$c"
	read ans
	case $ans in
	"") break;;
	 *) case $ans in
	    /*) eval $name=\$ans; break;;
	     *) echo; echo Error: $name should be an absolute path name. 1>&2;;
	    esac;;
	esac
    done
done

# In case some systems special-case pathnames beginning with //.

case $install_root in
/) install_root=
esac

# Load defaults from existing installation or from template main.cf file.

CONFIG_DIRECTORY=$install_root$config_directory

if [ -f $CONFIG_DIRECTORY/main.cf ]
then
    conf="-c $CONFIG_DIRECTORY"
else
    conf="-d"
fi

# Do not destroy parameter settings from environment or command line.

for name in daemon_directory command_directory queue_directory mail_owner \
    setgid_group sendmail_path newaliases_path mailq_path manpage_path
do
    eval : \${$name=\`bin/postconf $conf -h $name\`} || kill $$
done

# Grandfathering: if not in main.cf, get defaults from obsolete install.cf file.

grep setgid_group $CONFIG_DIRECTORY/main.cf >/dev/null 2>&1 || {
    if [ -f $CONFIG_DIRECTORY/install.cf ]
    then
	. $CONFIG_DIRECTORY/install.cf
	setgid_group=${setgid-$setgid_group}
	manpage_path=${manpages-$manpage_path}
    elif [ -n "$non_interactive" ]
    then
	echo Error: \"make upgrade\" requires the $CONFIG_DIRECTORY/main.cf 1>&2
	echo file from a sufficiently recent Postfix installation. 1>&2
	echo 1>&2
	echo Use \"make install\" instead. 1>&2
	exit 1
    fi
}

# Override default settings.

test -z "$non_interactive" && for name in daemon_directory command_directory \
    queue_directory sendmail_path newaliases_path mailq_path mail_owner \
    setgid_group manpage_path
do
    while :
    do
	echo
	eval echo Please specify \$${name}_prompt | fmt
	eval echo \$n "$name: [\$$name]\  \$c"
	read ans
	case $ans in
	"") break;;
	 *) eval $name=\$ans; break;;
	esac
    done
done

# Sanity checks

case $manpage_path in
 no) echo Error: manpage_path no longer accepts \"no\" values. 1>&2
     echo Error: re-run this script with \"make install\". 1>&2; exit 1;;
esac

case $setgid_group in
 no) echo Error: setgid_group no longer accepts \"no\" values. 1>&2
     echo Error: re-run this script with \"make install\". 1>&2; exit 1;;
esac

for path in $daemon_directory $command_directory \
    $queue_directory $sendmail_path $newaliases_path $mailq_path $manpage_path
do
   case $path in
   /*) ;;
    *) echo Error: $path should be an absolute path name. 1>&2; exit 1;;
   esac
done

test -d $tempdir || mkdir -p $tempdir || exit 1

( rm -f $tempdir/junk && touch $tempdir/junk ) || {
    echo Error: you have no write permission to $tempdir. 1>&2
    echo Specify an alternative directory for scratch files. 1>&2
    exit 1
}

chown root $tempdir/junk >/dev/null 2>&1 || {
    echo Error: you have no permission to change file ownership. 1>&2
    exit 1
}

chown "$mail_owner" $tempdir/junk >/dev/null 2>&1 || {
    echo Error: $mail_owner needs an entry in the passwd file. 1>&2
    echo Remember, $mail_owner must have a dedicated user id and group id. 1>&2
    exit 1
}

chgrp "$setgid_group" $tempdir/junk >/dev/null 2>&1 || {
    echo Error: $setgid_group needs an entry in the group file. 1>&2
    echo Remember, $setgid_group must have a dedicated group id. 1>&2
    exit 1
}

rm -f $tempdir/junk

# Avoid clumsiness.

DAEMON_DIRECTORY=$install_root$daemon_directory
COMMAND_DIRECTORY=$install_root$command_directory
QUEUE_DIRECTORY=$install_root$queue_directory
SENDMAIL_PATH=$install_root$sendmail_path
NEWALIASES_PATH=$install_root$newaliases_path
MAILQ_PATH=$install_root$mailq_path
MANPAGE_PATH=$install_root$manpage_path

# Create any missing directories.

test -d $CONFIG_DIRECTORY || mkdir -p $CONFIG_DIRECTORY || exit 1
test -d $DAEMON_DIRECTORY || mkdir -p $DAEMON_DIRECTORY || exit 1
test -d $COMMAND_DIRECTORY || mkdir -p $COMMAND_DIRECTORY || exit 1
test -d $QUEUE_DIRECTORY || mkdir -p $QUEUE_DIRECTORY || exit 1
for path in $SENDMAIL_PATH $NEWALIASES_PATH $MAILQ_PATH
do
    dir=`echo $path|sed -e 's/[/][/]*[^/]*$//' -e 's/^$/\//'`
    test -d $dir || mkdir -p $dir || exit 1
done

# Install files. Be careful to not copy over running programs.

for file in `censored_ls libexec`
do
    compare_or_replace a+x,go-w libexec/$file $DAEMON_DIRECTORY/$file || exit 1
done

for file in `censored_ls bin | grep '^post'`
do
    compare_or_replace a+x,go-w bin/$file $COMMAND_DIRECTORY/$file || exit 1
done

test -f bin/sendmail && {
    compare_or_replace a+x,go-w bin/sendmail $SENDMAIL_PATH || exit 1
    compare_or_symlink $SENDMAIL_PATH $NEWALIASES_PATH
    compare_or_symlink $SENDMAIL_PATH $MAILQ_PATH
}

if [ -f $CONFIG_DIRECTORY/main.cf ]
then
    for file in LICENSE `cd conf; censored_ls sample*` main.cf.default
    do
	compare_or_replace a+r,go-w conf/$file $CONFIG_DIRECTORY/$file || exit 1
    done
else
    for file in `cd conf; censored_ls * | grep -v postfix-script`
    do
	compare_or_replace a+r,go-w conf/$file $CONFIG_DIRECTORY/$file || exit 1
    done
    test -z "$install_root" && need_config=1
fi

# Save settings.

bin/postconf -c $CONFIG_DIRECTORY -e \
    "daemon_directory = $daemon_directory" \
    "command_directory = $command_directory" \
    "queue_directory = $queue_directory" \
    "mail_owner = $mail_owner" \
    "setgid_group = $setgid_group" \
    "sendmail_path = $sendmail_path" \
    "mailq_path = $mailq_path" \
    "newaliases_path = $newaliases_path" \
    "manpage_path = $manpage_path" \
|| exit 1

compare_or_replace a+x,go-w conf/postfix-script $CONFIG_DIRECTORY/postfix-script ||
    exit 1

# Install manual pages.

(cd man || exit 1
for dir in man?
    do test -d $MANPAGE_PATH/$dir || mkdir -p $MANPAGE_PATH/$dir || exit 1
done
for file in `censored_ls man?/*`
do
    (test -f $MANPAGE_PATH/$file && cmp -s $file $MANPAGE_PATH/$file &&
     echo Skipping $MANPAGE_PATH/$file...) || {
	echo Updating $MANPAGE_PATH/$file...
	rm -f $MANPAGE_PATH/$file
	cp $file $MANPAGE_PATH/$file || exit 1
	chmod 644 $MANPAGE_PATH/$file || exit 1
    }
done)

# Tighten access of existing directories.

for directory in pid
do
    test -d $QUEUE_DIRECTORY/$directory && {
	chown root $QUEUE_DIRECTORY/$directory || exit 1
    }
done

# Apply set-gid/group privileges for restricted access.

for directory in maildrop
do
    test -d $QUEUE_DIRECTORY/$directory || {
	mkdir -p $QUEUE_DIRECTORY/$directory || exit 1
	chown $mail_owner $QUEUE_DIRECTORY/$directory || exit 1
    }
    # Fix group and permissions if upgrading from world-writable maildrop.
    chgrp $setgid_group $QUEUE_DIRECTORY/$directory || exit 1
    chmod 730 $QUEUE_DIRECTORY/$directory || exit 1
done

for directory in public
do
    test -d $QUEUE_DIRECTORY/$directory || {
	mkdir -p $QUEUE_DIRECTORY/$directory || exit 1
	chown $mail_owner $QUEUE_DIRECTORY/$directory || exit 1
    }
    # Fix group and permissions if upgrading from world-accessible directory.
    chgrp $setgid_group $QUEUE_DIRECTORY/$directory || exit 1
    chmod 710 $QUEUE_DIRECTORY/$directory || exit 1
done

chgrp $setgid_group $COMMAND_DIRECTORY/postdrop $COMMAND_DIRECTORY/postqueue || exit 1
chmod g+s $COMMAND_DIRECTORY/postdrop $COMMAND_DIRECTORY/postqueue || exit 1

grep 'flush.*flush' $CONFIG_DIRECTORY/master.cf >/dev/null || {
	echo adding missing entry for flush service to master.cf
	cat >>$CONFIG_DIRECTORY/master.cf <<EOF
flush     unix  -       -       n       1000?   0       flush
EOF
}

grep "^pickup[ 	]*fifo[ 	]*n[ 	]*n" \
    $CONFIG_DIRECTORY/master.cf >/dev/null && {
	echo changing master.cf, making the pickup service unprivileged
	ed $CONFIG_DIRECTORY/master.cf <<EOF
/^pickup[ 	]*fifo[ 	]*n[ 	]*n/
s/\(n[ 	]*\)n/\1-/
p
w
q
EOF
}
for name in cleanup flush
do
    grep "^$name[ 	]*unix[ 	]*-" \
	$CONFIG_DIRECTORY/master.cf >/dev/null && {
	    echo changing master.cf, making the $name service public
	    ed $CONFIG_DIRECTORY/master.cf <<EOF
/^$name[ 	]*unix[ 	]*-/
s/-/n/
p
w
q
EOF
    }
done

found=`bin/postconf -c $CONFIG_DIRECTORY -h hash_queue_names`
missing=
(echo "$found" | grep active >/dev/null) || missing="$missing active"
(echo "$found" | grep bounce >/dev/null) || missing="$missing bounce"
(echo "$found" | grep defer >/dev/null)  || missing="$missing defer"
(echo "$found" | grep flush >/dev/null)  || missing="$missing flush"
(echo "$found" | grep incoming>/dev/null)|| missing="$missing incoming"
(echo "$found" | grep deferred>/dev/null)|| missing="$missing deferred"
test -n "$missing" && {
	echo fixing main.cf hash_queue_names for missing $missing
	bin/postconf -c $CONFIG_DIRECTORY -e hash_queue_names="$found$missing"
}

test "$need_config" = 1 || exit 0

ALIASES=`bin/postconf -h alias_database | sed 's/^[^:]*://'`
cat <<EOF 1>&2
    
    Warning: you still need to edit myorigin/mydestination/mynetworks
    in $CONFIG_DIRECTORY/main.cf. See also html/faq.html for dialup
    sites or for sites inside a firewalled network.
    
    BTW: Check your $ALIASES file and be sure to set up aliases
    for root and postmaster that direct mail to a real person, then
    run $NEWALIASES_PATH.

EOF

exit 0
