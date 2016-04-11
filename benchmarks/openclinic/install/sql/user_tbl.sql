/**
 * user_tbl.sql
 *
 * Creation of user_tbl structure
 *
 * Licensed under the GNU GPL. For full terms see the file LICENSE.
 *
 * @package   OpenClinic
 * @copyright 2002-2013 jact
 * @license   http://www.gnu.org/copyleft/gpl.html GNU GPL
 * @version   CVS: $Id: user_tbl.sql,v 1.7 2013/01/07 18:20:24 jact Exp $
 * @author    jact <jachavar@gmail.com>
 */

CREATE TABLE user_tbl (
  id_user INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  pwd VARCHAR(32) NOT NULL, /* 32 caracteres por ser md5 */
  email VARCHAR(40) NULL,
  actived ENUM('N','Y') NOT NULL DEFAULT 'N',
  id_theme SMALLINT UNSIGNED NOT NULL DEFAULT 1,
  id_profile SMALLINT UNSIGNED NOT NULL DEFAULT 3, /* por defecto perfil de m�dico */
  FOREIGN KEY (id_theme) REFERENCES theme_tbl(id_theme) ON DELETE SET DEFAULT,
  FOREIGN KEY (id_profile) REFERENCES profile_tbl(id_profile) ON DELETE SET DEFAULT
) ENGINE=MyISAM;

INSERT INTO user_tbl VALUES (
  1,
  '73850afb68a28c03ef4d2e426634e041',
  NULL,
  'Y',
  1,
  1
);

INSERT INTO user_tbl VALUES (
  2,
  '21232f297a57a5a743894a0e4a801fc3',
  NULL,
  'Y',
  1,
  1
);
