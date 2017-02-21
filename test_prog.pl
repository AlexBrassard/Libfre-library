#!/usr/bin/perl
use strict;
use warnings;

my $pattern;
my $tstring;
my $bo = 0;
my $eo = 0;

if ($#ARGV < 0){
    print("Error. Usage: test_prog.pl [pattern]\n\n");
    exit;
}
$pattern = shift;
if ($#ARGV < 0){
    $tstring = <STDIN>;
} else {
    $tstring = shift;
}

if ($tstring =~ /\Q$pattern/mg){
    print ("Match!\n");
    print ("$&\n");
}else{
    print ("No match.\n");
}
exit;
	
