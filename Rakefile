
MFTPD_BIN = "mftpd"
MFTPD_SRC = "mftpd.c"

MFTP_BIN = "mftp"
MFTP_SRC = "mftp.c"

task "default" => [MFTPD_BIN, MFTP_BIN]

file MFTPD_BIN => MFTPD_SRC do
  sh "gcc -Wall -Wextra -g3 -o #{MFTPD_BIN} #{MFTPD_SRC}"
end

file MFTP_BIN => MFTP_SRC do
  sh "gcc -Wall -Wextra -g3 -o #{MFTP_BIN} #{MFTP_SRC}"
end

task "cl" do
  mftpd_count = IO.readlines(MFTPD_SRC).size
  mftp_count = IO.readlines(MFTP_SRC).size
  puts "mftpd.c: #{mftpd_count}"
  puts "mftp.c: #{mftp_count}"
  puts "Total: #{mftpd_count + mftp_count}"
end
