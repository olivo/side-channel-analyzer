<?php

include_once(dirname(__FILE__) . '/dataflow_side_channel_analysis.php');
include_once(dirname(__FILE__) . '/TaintPHP/PHP-Parser-master/lib/bootstrap.php');
include_once(dirname(__FILE__) . '/TaintPHP/TaintAnalysis/TaintAnalysis.php');
include_once(dirname(__FILE__) . '/TaintPHP/CallGraph/CallGraph.php');
include_once(dirname(__FILE__) . '/TaintPHP/CFG/CFG.php');
include_once(dirname(__FILE__) . '/TaintPHP/CFG/FunctionSignature.php');
include_once(dirname(__FILE__) . '/TaintPHP/CFG/FunctionSignatureMap.php');

$projectPath = $argv[1];

// Iterating over all PHP files in a project path.
$Directory = new RecursiveDirectoryIterator($projectPath);
$Iterator = new RecursiveIteratorIterator($Directory);
$Regex = new RegexIterator($Iterator, '/^.+\.php$/i', RecursiveRegexIterator::GET_MATCH);
$Regex->rewind();

// Map from filenames to CFG information.
$cfgInfoMap = array();

// Map from function names to signatures.
$functionSignatures = new FunctionSignatureMap();

// Construct CFG map.
while($Regex->valid()) {
        // Regex iterator contains an array of a single element for each file.
        $fileName = $Regex->current()[0];
	
	// Obtain the CFGs of the main function, auxiliary functions and function signatures.
	$fileCFGInfo = CFG::construct_file_cfgs($fileName);
	$cfgInfoMap[$fileName] = $fileCFGInfo;

	$functionSignatures->addAll($fileCFGInfo->getFunctionRepresentations());

	$Regex->next();
}

// Construct call graphs, perform taint analysis and side channel detection.
$Regex->rewind();

print "==== FUNCTION SIGNATURES ===\n";
$functionSignatures->printFunctionSignatureMap();


// Add nodes of the call graphs from the global set of function signatures defined in the program.
// Analyze the entire program again to add edges and the nodes for non-user function calls.
$callGraph = new CallGraph();

$callGraph->addAllNodesFromFunctionSignatures($functionSignatures);

while($Regex->valid()) {

        $fileName = $Regex->current()[0];
        print "==== STARTING CALL GRAPH CONSTRUCTION: " . $fileName . " ====\n";
	$callGraph->addFileCallGraphInfo($cfgInfoMap[$fileName], $functionSignatures);

	print "Call Graph:\n";
	$callGraph->printCallGraph();

	//print "==== STARTING TAINT ANALYSIS ====\n";
	//$fileTaintedMaps = fileTaintAnalysis($fileCFGInfo);

	//print "==== STARTING SIDE-CHANNEL DETECTION ====\n";

	//dataflow_side_channel_detection($fileCFGInfo, $fileTaintedMaps);
	$Regex->next();
}

$callGraph->computeRootNodes();

$callGraph->printCallGraphRoots();

$callGraph->computeLeafNodes();

$callGraph->printCallGraphLeaves();

// Perform taint analysis over the entire application.
print "==== STARTING TAINT ANALYSIS ====\n";
$taintMap = taintAnalysis($callGraph, $cfgInfoMap, $functionSignatures);

// Perform side channel analysis over the entire application.
print "==== STARTING SIDE CHANNEL ANALYSIS ====\n";
sideChannelAnalysis($taintMap, $callGraph, $cfgInfoMap, $functionSignatures);
?>