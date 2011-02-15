#!/usr/bin/env perl

use strict;
use warnings;

use Test::BNFGen qw( :all );

def lc_letter => opt( 'a' .. 'z' );
def uc_letter => opt( 'A' .. 'Z' );
def letter    => opt( \'lc_letter', \'uc_letter' );
def word      => rep( \'lc_letter', 1, 20, 3 );

with_rules basic => sub {
  def
   sentence => seq( \'uc_letter', \'word' ),
   rep( seq( ' ', \'word' ), 0, 10, 3 ), '.';
  def var_char => opt( [ 26, \'letter' ], [ 10, \'digit' ] );
  def var_name => \'letter', rep( \'var_char', 0, 10, 3 );
  def digit => opt( '0' .. '9' );
  def one_to_nine => opt( '1' .. '9' );
  def number =>
   opt( '0', seq( \'one_to_nine', rep( \'digit', 0, 9, 3 ) ) );
  def assign => opt( 'LET ', '' ), \'var_name', ' = ', \'num_expr';
  def
   for_stmt => 'FOR ',
   \'var_name', ' = ', \'num_expr', ' TO ', \'num_expr';
  def next_stmt => 'NEXT';
  def for_next => \'for_stmt', "\n", \'program', \'next_stmt';
  def atom => opt( \'number', \'var_name' );
  def
   oper => \'num_expr',
   ' ', opt( '+', '-', '*', '/' ), ' ', \'num_expr';
  def paren => '(', \'num_expr', ')';
  def num_expr => opt( [ 5, \'atom' ], \'paren', \'oper' );
  def
   print_stmt => 'PRINT ',
   \'num_expr', rep( seq( opt( ', ', '; ' ), \'num_expr' ), 0, 3 );
  def statement =>
   opt( [ 5, \'assign' ], [ 5, \'print_stmt' ], \'for_next' );
  def line => \'statement', "\n";
  def program => rep( \'line', 0, 8 );

  print rule( 'program' )->();
};

with_rules xml => sub {
  def body => rep( \'xml', 0, 10, 2 );
  def attr => ' ', \'word', '="', \'word', '"';
  def attrs => rep( \'attr', 0, 5, 2 );

  def xml => opt(
    seq( '<', \'word', \'attrs', '/>' ),
    context(
      {
        tag => sub { const( \'word' ) }
      },
      '<',
      \'tag',
      \'attrs',
      '>',
      \'body',
      '</',
      \'tag',
      '>'
    )
  );

  print rule( 'xml' )->(), "\n";
};

# vim:ts=2:sw=2:sts=2:et:ft=perl
