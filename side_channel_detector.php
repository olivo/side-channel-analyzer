<?php

include_once(dirname(__FILE__) . '/dataflow_side_channel_analysis.php');
include_once(dirname(__FILE__) . '/TaintPHP/PHP-Parser-master/lib/bootstrap.php');
include_once(dirname(__FILE__) . '/TaintPHP/TaintAnalysis/taint_analysis.php');
include_once(dirname(__FILE__) . '/TaintPHP/CFG/CFG.php');

$filename = $argv[1];
	
// Obtain the CFGs of the main function, auxiliary functions and function signatures.
$file_cfgs = CFG::construct_file_cfgs($filename);

print "==== STARTING TAINT ANALYSIS ====\n";
$tainted_maps = taint_analysis($file_cfgs[0], $file_cfgs[1], $file_cfgs[2]);

print "==== STARTING SIDE-CHANNEL DETECTION ====\n";

dataflow_side_channel_detection($file_cfgs[0], $file_cfgs[1], $file_cfgs[2]);

?>