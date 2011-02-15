package Test::BNFGen;

use strict;
use warnings;

use Data::Dumper;
use List::Util qw( sum );
use Scalar::Util qw( refaddr );

# TODO Fully evaluate all args - so that, e.g. the parameters to rep can
# be runtime variables.

# TODO Implement generators

use base qw( Exporter );

our @EXPORT_OK = qw(
 ci const context def opt rep rule seq
);

our %EXPORT_TAGS = ( all => \@EXPORT_OK, );

use constant MAX_DEPTH => 10;

my %Rule = ();

=head1 NAME

Test::BNFGen - Generate random data complying with BNF style syntaxes

=cut

# Accepts a list of options of this form:
#
# [ probability, action ]
#
# Probability is an integer - no upper bound is defined
#
# action is one of
#
#   * a scalar          - the action returns that value
#   * a coderef         - the action is invoked and its return value
#                         used

sub def(@) {
  my ( $name, @code ) = @_;
  $Rule{$name} = seq( @code );
}

sub ci($) {
  my @part = split /\b/, shift;
  my @seq = ();
  for my $p ( @part ) {
    if ( $p =~ /\w/ ) {
      push @seq, map { opt( lc $_, uc $_ ) } split //, $p;
    }
    else {
      push @seq, opt( $p );
    }
  }
  return seq( @seq );
}

sub const($) {
  my $v = seq( @_ )->();
  return sub { $v };
}

sub context($@) {
  my ( $ctx, @seq ) = @_;
  my %bind  = %$ctx;
  my %stash = ();
  return sub {
    local @Rule{ keys %bind };
    while ( my ( $k, $v ) = each %bind ) {
      $Rule{$k} = opt( $v->() );
    }
    return seq( @seq )->();
   }
}

sub _limit($) {
  my $cb = shift;
  my $limiter ||= do {
    my $depth = 0;
    sub {
      my $cb = shift;
      ++$depth;
      if ( $depth >= MAX_DEPTH ) {
        --$depth;
        #        warn "DEPTH LIMIT\n";
        return ' *** ';
      }
      my $rv = $cb->();
      --$depth;
      return $rv;
    };
  };
  return $cb if refaddr $cb == refaddr $limiter;
  return sub { $limiter->( $cb ) };
}

sub norm($);
sub norm($) {
  my $opt = shift;
  return norm( [ 1, $opt ] ) unless 'ARRAY' eq ref $opt;
  if ( ref $opt->[1] ) {
    if ( 'SCALAR' eq ref $opt->[1] ) {
      my $v = ${ $opt->[1] };
      $opt->[1] = rule( $v );
    }
    elsif ( 'CODE' ne ref $opt->[1] ) {
      die "Not a code or scalar reference";
    }
  }
  else {
    my $v = $opt->[1];
    $opt->[1] = sub { $v };
  }
  $opt->[1] = _limit( $opt->[1] );
  return $opt;
}

sub opt(@) {
  my @opt = map { norm( $_ ) } @_;
  die "No options to switch between" if @opt == 0;
  return $opt[0][1] if @opt == 1;

  my @pick   = ();
  my $last_p = 0;
  for my $opt ( @opt ) {
    push @pick, [ $last_p, $opt->[1] ];
    $last_p += $opt->[0];
  }

  push @pick, [ $last_p, sub { die "Sentinel" } ];

  return sub {
    my $slot = rand $last_p;
    my ( $lo, $hi ) = ( 0, $#pick );
    while ( $lo < $hi ) {
      my $mid = int( ( $lo + $hi ) / 2 );
      if ( $slot < $pick[$mid][0] ) {
        $hi = $mid;
      }
      elsif ( $slot >= $pick[ $mid + 1 ][0] ) {
        $lo = $mid + 1;
      }
      else {
        return $pick[$mid][1]->();
      }
    }
    die "Can't find $slot. Shouldn't happen.";
  };
}

sub seq(@) {
  my @seq = map { opt( $_ ) } @_;
  return sub { '' }
   if @seq == 0;
  return $seq[0] if @seq == 1;
  return sub {
    join '', map { $_->() } @seq;
  };
}

sub _pow_rand {
  my ( $limit, $power ) = @_;
  rand( $limit**( 1 / $power ) )**$power;
}

sub rep(@) {
  my $cb = opt( shift );
  my ( $min, $max, $pow ) = @_;
  $pow = 1 unless defined $pow;
  return sub {
    join '',
     map { $cb->() } 1 .. $min + _pow_rand( $max + 1 - $min, $pow );
  };
}

sub rule(@) {
  my $name = shift;
  return sub {
    ( $Rule{$name} || die "Rule $name not defined\n" )->();
  };
}

1;

# vim:ts=2:sw=2:sts=2:et:ft=perl
