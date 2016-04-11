/**
 * problem_tbl.sql
 *
 * Creation of problem_tbl structure
 *
 * Licensed under the GNU GPL. For full terms see the file LICENSE.
 *
 * @package   OpenClinic
 * @copyright 2002-2013 jact
 * @license   http://www.gnu.org/copyleft/gpl.html GNU GPL
 * @version   CVS: $Id: problem_tbl.sql,v 1.8 2013/01/07 18:18:50 jact Exp $
 * @author    jact <jachavar@gmail.com>
 */

CREATE TABLE problem_tbl (
  id_problem INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  last_update_date DATE NOT NULL, /* fecha de �ltima actualizaci�n */
  id_patient INT UNSIGNED NOT NULL,
  id_member INT UNSIGNED NULL, /* clave del m�dico que atiende el problema */
  order_number TINYINT UNSIGNED NOT NULL, /* n�mero de orden relativo a cada paciente */
  opening_date DATE NOT NULL, /* fecha de apertura */
  closing_date DATE NULL, /* fecha de cierre */
  meeting_place VARCHAR(40) NULL, /* lugar de encuentro */
  wording TEXT NOT NULL, /* enunciado del problema */
  subjective TEXT NULL, /* subjetivo */
  objective TEXT NULL, /* objetivo */
  appreciation TEXT NULL, /* valoraci�n */
  action_plan TEXT NULL, /* plan de actuaci�n */
  prescription TEXT NULL, /* prescripci�n (por prescripci�n facultativa, on doctor's orders) */
  FOREIGN KEY (id_patient) REFERENCES patient_tbl(id_patient) ON DELETE CASCADE,
  FOREIGN KEY (id_member) REFERENCES staff_tbl(id_member) ON DELETE SET NULL
) ENGINE=MyISAM;
