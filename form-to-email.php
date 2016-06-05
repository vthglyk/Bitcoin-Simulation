<?php

  $subject = $_POST['subject'];
  $visitor_email = $_POST['email'];
  $message = $_POST['message'];
  $email_from = 'vthglyk@gmail.com';
  
  $to = "vthglyk@gmail.com";
 
  $headers = "From: $email_from \r\n";
 
  $headers .= "Reply-To: $visitor_email \r\n";
 
  mail($to,$email_subject,$email_body,$headers);
  
  echo "E-mail was sent successfully
?>