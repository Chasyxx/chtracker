my $input = <STDIN>;
$input =~ s/-\w(?=\S+)//;
$input =~ s/(?<=\s|^)-\S+//;
chomp($input);
$input =~ s/^\s+|\s+$//;
print $input;