#!/bin/sh

# Sample Postfix installation script. Run this from the top-level
# Postfix source directory.

PATH=/bin:/usr/bin:/usr/sbin:/usr/etc:/sbin:/etc:/usr/contrib/bin:/usr/gnu/bin:/usr/ucb:/usr/bsd
umask 022

test -t 0 && cat <<EOF

Warning: this script replaces existing sendmail or Postfix programs.
Make backups if you want to be able to recover.

Before installing files, this script prompts you for some definitions.
Most definitions will be remembered, so you have to specify them
only once. All definitions have a reasonable default value.
EOF

install_root_text="the prefix for installed file names. This is
useful only if you are building ready-to-install packages for other
machines."

tempdir_text="directory for scratch files while installing Postfix.
You must must have write permission in this directory."

config_directory_text="the directory with Postfix configuration
files. For security reasons this directory must be owned by the
super-user."

daemon_directory_text="the directory with Postfix daemon programs.
This directory should not be in the command search path of any
users."

command_directory_text="the directory with Postfix administrative
commands.  This directory should be in the command search path of
adminstrative users."

queue_directory_text="the directory with Postfix queues."

sendmail_path_text="the full pathname of the Postfix sendmail
command. This is the Sendmail-compatible mail posting interface."

newaliases_path_text="the full pathname of the Postfix newaliases
command.  This is the Sendmail-compatible command to build alias
databases."

mailq_path_text="the full pathname of the Postfix mailq command.
This is the Sendmail-compatible mail queue listing command."

mail_owner_text="the owner of the Postfix queue. Specify a user
account with numerical user ID and group ID values that are not
used by any other user accounts."

setgid_text="the group for mail submission and queue management
commands.  Specify a group name with a numerical group ID that is
not shared with other accounts, not even with the Postfix account."

manpages_text="where to install the Postfix on-line manual pages."

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

# Default settings. Most are clobbered by remembered settings.

: ${install_root=/}
: ${tempdir=`pwd`}
: ${config_directory=/etc/postfix}
: ${daemon_directory=/usr/libexec/postfix}
: ${command_directory=/usr/sbin}
: ${queue_directory=/var/spool/postfix}
if [ -f /usr/lib/sendmail ]
    then : ${sendmail_path=/usr/lib/sendmail}
    else : ${sendmail_path=/usr/sbin/sendmail}
fi
: ${newaliases_path=/usr/bin/newaliases}
: ${mailq_path=/usr/bin/mailq}
: ${mail_owner=postfix}
: ${setgid=postdrop}
: ${manpages=/usr/local/man}

# Find out the location of configuration files.

test -t 0 &&
for name in install_root tempdir config_directory
do
    while :
    do
	echo
	eval echo Please specify \$${name}_text | fmt
	eval echo \$n "$name: [\$$name]\  \$c"
	read ans
	case $ans in
	"") break;;
	 *) eval $name=\$ans; break;;
	esac
    done
done

# Sanity checks

for path in $tempdir $install_root $config_directory
do
   case $path in
   /*) ;;
    *) echo Error: $path should be an absolute path name. 1>&2; exit 1;;
   esac
done

# In case some systems special-case pathnames beginning with //.

case $install_root in
/) install_root=
esac

# Load defaults from existing installation.

CONFIG_DIRECTORY=$install_root$config_directory

test -f $CONFIG_DIRECTORY/main.cf && {
    for name in daemon_directory command_directory queue_directory mail_owner 
    do
	eval $name='"`bin/postconf -c $CONFIG_DIRECTORY -h $name`"' || kill $$
    done
}

if [ -f $CONFIG_DIRECTORY/install.cf ]
then
    . $CONFIG_DIRECTORY/install.cf
elif [ ! -t 0 -a -z "$install_root" ]
then
    echo Non-interactive install needs the $CONFIG_DIRECTORY/install.cf 1>&2
    echo file from a previous Postfix installation. 1>&2
    echo 1>&2
    echo Use interactive installation instead. 1>&2
    exit 1
fi

# Override default settings.

test -t 0 &&
for name in daemon_directory command_directory \
    queue_directory sendmail_path newaliases_path mailq_path mail_owner\
    setgid manpages
do
    while :
    do
	echo
	eval echo Please specify \$${name}_text | fmt
	eval echo \$n "$name: [\$$name]\  \$c"
	read ans
	case $ans in
	"") break;;
	 *) eval $name=\$ans; break;;
	esac
    done
done

# Sanity checks

case $manpages in
 no) echo Error: manpages no longer accepts "no" values. 1>&2
     echo Error: re-run this script in interactive mode. 1>&2; exit 1;;
esac

case $setgid in
 no) echo Error: setgid no longer accepts "no" values. 1>&2
     echo Error: re-run this script in interactive mode. 1>&2; exit 1;;
esac

for path in $daemon_directory $command_directory \
    $queue_directory $sendmail_path $newaliases_path $mailq_path $manpages
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

chgrp "$setgid" $tempdir/junk >/dev/null 2>&1 || {
    echo Error: $setgid needs an entry in the group file. 1>&2
    echo Remember, $setgid must have a dedicated group id. 1>&2
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
MANPAGES=$install_root$manpages

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
    cp `censored_ls conf/*` $CONFIG_DIRECTORY || exit 1
    chmod a+r,go-w $CONFIG_DIRECTORY/* || exit 1

    test -z "$install_root" && need_config=1
fi

# Save settings.

bin/postconf -c $CONFIG_DIRECTORY -e \
    "daemon_directory = $daemon_directory" \
    "command_directory = $command_directory" \
    "queue_directory = $queue_directory" \
    "mail_owner = $mail_owner" \
|| exit 1

(echo "# This file was generated by $0"
for name in sendmail_path newaliases_path mailq_path setgid manpages
do
    eval echo $name=\$$name
done) >$tempdir/junk || exit 1
compare_or_move a+x,go-w $tempdir/junk $CONFIG_DIRECTORY/install.cf || exit 1
rm -f $tempdir/junk

compare_or_replace a+x,go-w conf/postfix-script $CONFIG_DIRECTORY/postfix-script ||
    exit 1

# Install manual pages.

(cd man || exit 1
for dir in man?
    do test -d $MANPAGES/$dir || mkdir -p $MANPAGES/$dir || exit 1
done
for file in `censored_ls man?/*`
do
    (test -f $MANPAGES/$file && cmp -s $file $MANPAGES/$file &&
     echo Skipping $MANPAGES/$file...) || {
	echo Updating $MANPAGES/$file...
	rm -f $MANPAGES/$file
	cp $file $MANPAGES/$file || exit 1
	chmod 644 $MANPAGES/$file || exit 1
    }
done)

# Use set-gid/group privileges for restricted access.

for directory in maildrop
do
    test -d $QUEUE_DIRECTORY/$directory || {
	mkdir -p $QUEUE_DIRECTORY/$directory || exit 1
	chown $mail_owner $QUEUE_DIRECTORY/$directory || exit 1
    }
    # Fix group if upgrading from world-writable maildrop.
    chgrp $setgid $QUEUE_DIRECTORY/$directory || exit 1
    chmod 730 $QUEUE_DIRECTORY/$directory || exit 1
done

for directory in public
do
    test -d $QUEUE_DIRECTORY/$directory || {
	mkdir -p $QUEUE_DIRECTORY/$directory || exit 1
	chown $mail_owner $QUEUE_DIRECTORY/$directory || exit 1
    }
    # Fix group if upgrading from world-accessible directory.
    chgrp $setgid $QUEUE_DIRECTORY/$directory || exit 1
    chmod 710 $QUEUE_DIRECTORY/$directory || exit 1
done

for directory in pid
do
    test -d $QUEUE_DIRECTORY/$directory && {
	chown root $QUEUE_DIRECTORY/$directory || exit 1
    }
done

chgrp $setgid $COMMAND_DIRECTORY/postdrop $COMMAND_DIRECTORY/postqueue || exit 1
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
