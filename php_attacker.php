<?php

include_once "PHP-Parser-master/lib/bootstrap.php";

include_once "security_constraints.php";
include_once "symbolic_execution.php";

include_once "CFG.php";

$filename = $argv[1];
	
// Obtain the CFGs of the main function, auxiliary functions and function signatures.
$file_cfgs = CFG::construct_file_cfgs($filename);

// Perform symbolic execution on the main cfg.
symbolic_execute($file_cfgs[0],$file_cfgs[1],$file_cfgs[2]);


?>