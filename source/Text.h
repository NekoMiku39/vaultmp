#ifndef TEXTGUI_H
#define TEXTGUI_H

#include "vaultmp.h"
#include "Window.h"

/**
 * \brief Represents a GUI static text
 */

class Text : public Window
{
		friend class GameFactory;

	private:
		void initialize();

		Text(const Text&);
		Text& operator=(const Text&);

	protected:
		Text();
		Text(const pDefault* packet);
		Text(pPacket&& packet) : Text(packet.get()) {};

	public:
		virtual ~Text() noexcept;

		/**
		 * \brief For network transfer
		 */
		virtual pPacket toPacket() const;
};

#endif
