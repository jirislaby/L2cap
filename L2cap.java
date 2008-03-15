import javax.microedition.lcdui.Command;
import javax.microedition.lcdui.CommandListener;
import javax.microedition.lcdui.Display;
import javax.microedition.lcdui.Displayable;
import javax.microedition.lcdui.Form;
import javax.microedition.lcdui.Spacer;
import javax.microedition.lcdui.TextField;

import javax.microedition.lcdui.Item;
import javax.microedition.lcdui.StringItem;

import javax.microedition.midlet.MIDlet;

public class L2cap extends MIDlet implements CommandListener
{
	private Form form;
	private TextField word;
	private StringItem word1;
	private Command trans, exit;

	public L2cap() {
		form = new Form("Slovník");

		form.append(word = new TextField("Slovo", null, 30,
					TextField.ANY));
		form.append(word1 = new StringItem("Překlad\n", null));
		form.addCommand(trans = new Command("Přelož", Command.OK, 1));
		form.addCommand(exit = new Command("Konec", Command.EXIT, 2));
		form.setCommandListener(this);
	}

	public void startApp() {
		Display.getDisplay(this).setCurrent(form);
	}

	public void pauseApp() {
	}

	public void destroyApp(boolean unconditional) {
	}

	public void commandAction(Command c, Displayable d) {
		if (c == trans)
			word1.setText("aa");
		else if (c == exit) {
			destroyApp(false);
			notifyDestroyed();
		}
	}
}
