<?php

include_once(dirname(__FILE__) . '/dataflow_side_channel_analysis.php');
include_once(dirname(__FILE__) . '/TaintPHP/PHP-Parser-master/lib/bootstrap.php');
include_once(dirname(__FILE__) . '/TaintPHP/TaintAnalysis/taint_analysis.php');
include_once(dirname(__FILE__) . '/TaintPHP/CFG/CFG.php');

$projectPath = $argv[1];

// Iterating over all php files in a project path.
$Directory = new RecursiveDirectoryIterator($projectPath);
$Iterator = new RecursiveIteratorIterator($Directory);
$Regex = new RegexIterator($Iterator, '/^.+\.php$/i', RecursiveRegexIterator::GET_MATCH);
$Regex->rewind();

while($Regex->valid()) {
        // Regex iterator contains an array of a single element for each file.
        $fileName = $Regex->current()[0];
	print "==== STARTING ANALYSIS ON FILE: " . $fileName . "\n";
	
	// Obtain the CFGs of the main function, auxiliary functions and function signatures.
	$file_cfgs = CFG::construct_file_cfgs($fileName);

	//print "==== STARTING TAINT ANALYSIS ====\n";
	//$file_tainted_maps = taint_analysis($file_cfgs[0], $file_cfgs[1], $file_cfgs[2]);

	//print "==== STARTING SIDE-CHANNEL DETECTION ====\n";

	//dataflow_side_channel_detection($file_cfgs[0], $file_cfgs[1], $file_cfgs[2], $file_tainted_maps);
	$Regex->next();
}

?>