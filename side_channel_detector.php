<?php

include_once(dirname(__FILE__) . '/dataflow_side_channel_analysis.php');
include_once(dirname(__FILE__) . '/TaintPHP/PHP-Parser-master/lib/bootstrap.php');
include_once(dirname(__FILE__) . '/TaintPHP/TaintAnalysis/taint_analysis.php');
include_once(dirname(__FILE__) . '/TaintPHP/CallGraph/CallGraph.php');
include_once(dirname(__FILE__) . '/TaintPHP/CFG/CFG.php');
include_once(dirname(__FILE__) . '/TaintPHP/CFG/FunctionSignature.php');

$projectPath = $argv[1];

// Iterating over all php files in a project path.
$Directory = new RecursiveDirectoryIterator($projectPath);
$Iterator = new RecursiveIteratorIterator($Directory);
$Regex = new RegexIterator($Iterator, '/^.+\.php$/i', RecursiveRegexIterator::GET_MATCH);
$Regex->rewind();

// Map from filenames to CFG information.
$cfgInfoMap = array();

// Map from function names to signatures.
$functionSignatures = array();

// Construct CFG map.
while($Regex->valid()) {
        // Regex iterator contains an array of a single element for each file.
        $fileName = $Regex->current()[0];
	
	// Obtain the CFGs of the main function, auxiliary functions and function signatures.
	$fileCFGInfo = CFG::construct_file_cfgs($fileName);
	$cfgInfoMap[$fileName] = $fileCFGInfo;
	
	addFunctionSignatures($functionSignatures, $fileCFGInfo->getFunctionRepresentations());

	$Regex->next();
}

// Construct call graphs, perform taint analysis and side channel detection.
$Regex->rewind();

print "==== FUNCTION SIGNATURES ===\n";
foreach($functionSignatures as $name => $signatures) {
      print "Name: " . $name . "\n";
      print "Signatures: ";
      foreach($signatures as $signature) {
          print $signature->printFunctionSignature() . ", ";
      }
      print "\n";
}


$callGraph = new CallGraph();

while($Regex->valid()) {

        $fileName = $Regex->current()[0];
        print "==== STARTING CALL GRAPH CONSTRUCTION: " . $fileName . " ====\n";
	$callGraph->addFileCallGraphInfo($cfgInfoMap[$fileName]);

	print "Call Graph:\n";
	$callGraph->printCallGraph();

	//print "==== STARTING TAINT ANALYSIS ====\n";
	//$file_tainted_maps = taint_analysis($fileCFGInfo);

	//print "==== STARTING SIDE-CHANNEL DETECTION ====\n";

	//dataflow_side_channel_detection($fileCFGInfo, $file_tainted_maps);
	$Regex->next();
}

// Function that adds the function signatures to the global map of function signatures.
function addFunctionSignatures($functionSignatureMap, $functionSignatures) {
       
       foreach($functionSignatures as $name => $signature) {
           if(!isset($functionSignatureMap[$name])) {
	       $functionSignatureMap = array();
	   }
	   $functionSignatureMap[$name][] = $signature;
       }
}


?>