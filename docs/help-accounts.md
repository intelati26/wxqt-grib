# Accounts and keys

Settings > **Accounts** holds the logins some data needs. The program works without them; a source that needs one says so.

## NASA Earthdata Login token

Some data is given only to a logged-in request: PO.DAAC's ASCAT ocean winds, for one. The account is free
(<https://urs.earthdata.nasa.gov/>). On its page, **Generate Token** makes a token, which lasts about 60 days. Paste it into the
box (a long line of letters, digits and dots) and press save: it is kept in this program's settings file, which stays through
upgrades, and **sent only to NASA's Earthdata addresses**, never to any other site. **Earthdata login...** opens that page in your
browser. The test button asks NASA whether it accepts the token ("NASA accepted the token"). When the token expires, paste the new
one; **Remove** deletes it from the settings. **Save** keeps what is in the box.

## Your contact email

Some data servers ask automated programs to say who they are. The program asks for a contact address at the start until you save
one; it goes only into the program's **User-Agent** header of requests to those servers, never anywhere else, and is not used for
anything else.

## Where they are kept

In the program's settings file on this computer, in plain text: anyone who can read your files can read it. Do not use a token you
would not want shared on a computer that others use.
