#!/usr/bin/env perl

use strict;
use warnings;

use Data::Dumper;
use List::Util qw( max );

my %decl = ();

my $bw = qr{
  (?: (?<=\s) | ^ ) 
  ( [-a-z_]+ ) 
  (?: (?=\s) | $ )
}x;

my $curdecl = undef;
while ( defined( my $line = <> ) ) {
  chomp $line;
  next if $line =~ /^\s*$/;
  next if $line =~ /^\s*\%/;
  $line =~ s/^(\w+):/$1 :/;
  $line =~ s/$bw/fix_id($1)/eg;
  if ( $line =~ m{^ $bw \s* : \s* (.*) }x ) {
    $curdecl = $1;
    my $tail = $2;
    die "$.: $curdecl already defined\n" if $decl{$curdecl};
    $decl{$curdecl} = {
      lines => [],
      deps  => {},
    };
    push_decl( $curdecl, $tail );
  }
  elsif ( $line =~ m{^ \s* \| \s* (.*) }x ) {
    die "$.: No current declaration\n" unless defined $curdecl;
    push_decl( $curdecl, $1 );
  }
  elsif ( $line =~ m{^ \s* \; \s* $ }x ) {
    undef $curdecl;
  }
  else {
    die "$.: Bad line\n";
  }
}

my $maxname = max map { length $_ } keys %decl;
my $fmt     = "%-${maxname}s %-2s %s\n";
my $done    = {};
for my $name ( sort keys %decl ) {
  show_decl( $name, $done );
}

sub show_decl {
  my ( $name, $done ) = @_;
  return if $done->{$name}++;
  my $info = $decl{$name};
  for my $dep ( sort keys %{ $info->{deps} } ) {
    show_decl( $dep, $done );
  }
  my $op = '=';
  for my $ln ( @{ $info->{lines} } ) {
    printf $fmt, $name, $op, $ln;
    $op = '=/';
  }
  print "\n" unless $op eq '=';
}

sub push_decl {
  my ( $name, $tail ) = @_;
  while ( $tail =~ m{$bw}g ) {
    $decl{$name}{deps}{$1}++;
  }
  $tail =~ s/\s+\|\s+/ \/ /g;
  push @{ $decl{$name}{lines} }, $tail;
}

sub fix_id {
  my $id = shift;
  $id =~ s/_/-/g;
  return $id;
}

# vim:ts=2:sw=2:sts=2:et:ft=perl

