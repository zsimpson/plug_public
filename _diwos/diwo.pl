
####################################################################################################
#
# HASH pack and unpack

use Scalar::Util;

sub hashUnpack {
	my( $readBuf ) = @_;
	my %hash;

	($len, $recCount) = unpack( 'll', $readBuf );

	$readBuf = substr( $readBuf, 8 );

	for( $i=0; $i<$recCount; $i++ ) {
		($type, $keyLen, $valLen) = unpack( 'lll', $readBuf );
		$keyPad = (4-($keyLen&3)) & 3;
		$valPad = (4-($valLen&3)) & 3;

		$readBuf = substr( $readBuf, 12 );

		$key = substr( $readBuf, 0, $keyLen-1 );
		$val = substr( $readBuf, $keyLen+$keyPad, $valLen );

		$readBuf = substr( $readBuf, $keyLen+$keyPad+$valLen+$valPad );

		if( $type == 1 ) {
			# BIN
		}
		elsif( $type == 2 ) {
			# STR
			$val = substr( $val, 0, $valLen-1 );
		}
		elsif( $type == 3 ) {
			# S32
			$val = unpack( 'l', $val );
		}
		elsif( $type == 4 ) {
			# U32
			$val = unpack( 'L', $val );
		}
		elsif( $type == 5 ) {
			# S64
			die "int64";
		}
		elsif( $type == 6 ) {
			# FLT
			$val = unpack( 'f', $val );
		}
		elsif( $type == 7 ) {
			# DBL
			$val = unpack( 'd', $val );
		}
		elsif( $type == 8 ) {
			# PTR
			die "ptr type";
		}
		elsif( $type == 9 ) {
			# PFR
			die "ptr type";
		}

		$hash{$key} = $val;
	}

	return %hash;
}

sub hashPack {
	my( %hash ) = %{$_[0]};

	# PACK the hashtable
	my $msg;
	my $recordCount = 0;
	while( my($key,$val) = each %hash ) {

		my $isNum   = Scalar::Util::looks_like_number( $val );
		my $isFloat = $isNum & 4;
		my $isNeg   = $isNum & (32|16);
		my $isWeird = $isNum & 8;

		if( !$isNum || $isWeird ) {
			# TYPE: 2=string
			$msg .= pack( 'l', 2 );

			# KEY and VAL length
			$keyLen = length( $key ) + 1;
			$keyPad = (4-($keyLen&3)) & 3;
			$msg .= pack( 'l', $keyLen );
			$valLen = length( $val ) + 1;
			$valPad = (4-($valLen&3)) & 3;
			$msg .= pack( 'l', $valLen );

			# KEY and padding
			$msg .= $key;
			$msg .= pack( 'x' x (1+$keyPad) );

			# VAL and padding
			$msg .= $val;
			$msg .= pack( 'x' x (1+$valPad) );
		}
		elsif( $isFloat ) {
			# TYPE: 7=double
			$msg .= pack( 'l', 7 );

			# KEY and VAL length
			$keyLen = length( $key ) + 1;
			$keyPad = (4-($keyLen&3)) & 3;
			$msg .= pack( 'l', $keyLen );
			$valLen = 8;
			$valPad = 0;
			$msg .= pack( 'l', $valLen );

			# KEY and padding
			$msg .= $key;
			$msg .= pack( 'x' x (1+$keyPad) );

			# VAL and padding
			$msg .= pack( 'd', $val );
		}
		elsif( $isNeg ) {
			# TYPE: 3=signed 32
			$msg .= pack( 'l', 3 );

			# KEY and VAL length
			$keyLen = length( $key ) + 1;
			$keyPad = (4-($keyLen&3)) & 3;
			$msg .= pack( 'l', $keyLen );
			$valLen = 4;
			$valPad = 0;
			$msg .= pack( 'l', $valLen );

			# KEY and padding
			$msg .= $key;
			$msg .= pack( 'x' x (1+$keyPad) );

			# VAL and padding
			$msg .= pack( 'l', $val );
		}
		else {
			# TYPE: 4=unsigned 32
			$msg .= pack( 'l', 4 );

			# KEY and VAL length
			$keyLen = length( $key ) + 1;
			$keyPad = (4-($keyLen&3)) & 3;
			$msg .= pack( 'l', $keyLen );
			$valLen = 4;
			$valPad = 0;
			$msg .= pack( 'l', $valLen );

			# KEY and padding
			$msg .= $key;
			$msg .= pack( 'x' x (1+$keyPad) );

			# VAL and padding
			$msg .= pack( 'L', $val );
		}

		$recordCount++;
	}

	my $len = length( $msg );

	# PREPEND the packed hash table header (length including this header, count)
	$msg = pack( "ll", $len+8, $recordCount ) . $msg;

	return $msg;
}


####################################################################################################
#
# CONNECT

$address = "zbs.homeip.net:59997";

use Socket;
socket( S, PF_INET, SOCK_STREAM, getprotobyname("tcp") );
($addr,$port) = split( /:/, $address );
$port = 59997 if !$port;
$addr = gethostbyname($addr);
$addr = sockaddr_in( $port, $addr );
$c = connect( S, $addr );
if( $c <= 0 ) {
	print "Client failed connect\n";
	exit;
}

print "Connected\n";

sub sendZMsg {
	my( %hash ) = %{$_[0]};

	my $msg = hashPack( \%hash );

	# PREPEND header for the packet
	$msg = pack( "ll", 2, length($msg) ) . $msg;
		# 2 means it is a binary coded hash
		# followed by the length not including this header

	send( S, $msg, 0 );
}

sub readZMsg {
	my $readBuf;

	my $r = read( S, $readBuf, 8, 0 );
	my($packetType, $len) = unpack( 'll', $readBuf );

	die "unknown type" if $packetType != 2;

	$r = read( S, $readBuf, $len, 0 );

	%hash = hashUnpack( $readBuf );
	return %hash;
}


####################################################################################################
#
# SEND a message

%sendMsg = (
	type => CommandActs,
);

sendZMsg( \%sendMsg );


####################################################################################################
#
# READ a message

%replyMsg = readZMsg();

foreach( sort keys( %replyMsg ) ) {
	print "$_ $replyMsg{$_}\n";
}

#while( ($key,$val) = each %replyMsg ) {
#	print "key='$key' val='$val'\n";
#}


