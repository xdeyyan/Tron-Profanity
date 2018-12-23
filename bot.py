import discord
from discord.ext.commands import Bot
from discord.ext import commands
import asyncio
import time

Client = discord.Client()
client = commands.Bot(command_prefix = "!")

@client.event
async def on_ready():
    print("Učitavam Discord API")
    print("Povezivanje na servere....")
    print("Provjera tokena----")
    print("'Snješko bot' je spreman!")

@client.event
async def on_message(message):
    if message.content.upper() == "SULJO":
        await client.send_message(message.channel, ":pig: @Mifke, gdje si?")
    if message.content.upper().startswith('!ZULA'):
        userID = message.author.id
        await client.send_message(message.channel, "<@%s>, najbolji igrač je 'mitric[51]' " % (userID))
    if message.content.upper().startswith('!KOMANDE'):
        await client.send_message(message.channel, "Komande su : \n !zula - Prikaz najboljeg igrača (level) \n !napisi - Poruka u ime bota \n !sajt - Link sajta \n !sajt - Lista igrica \n ADMIN komande: \n !broadcast - Boldirana poruka na ekranu \n !ban - Banuj \n !unban - Unban \n !kick - Kickuj \n !mute - Muteuj ")
    if message.content.upper().startswith('!NAPISI'):
        args = message.content.split(" ")
        await client.send_message(message.channel, "%s" % (" ".join(args[1:])))
    if message.content.upper().startswith('!SAJT'):
        userID = message.author.id
        await client.send_message(message.channel, "<@%s>, posjeti link 'https://www.snjesko.ga/' " % (userID))
     if message.content.upper().startswith('!IGRICE')
        await client.send_message(message.channel, "Zula Europe \n Counter-Strike: Global Offensive \n Grand Theft Auto V \n Fortnite \n Far Cry 5 \n Subnautica \n Kerbal Space Program")   
@client.command(pass_context=True)
async def obrisi(ctx, amount=100):
    channel = ctx.message.channel
    messages = []
    async for message in client.logs_from(channel, limit=int(amount)):
        messages.append(message)
    await client.delete_message(messages)
    await client.say('Poruke obrisane.')
    
 #Sistem moderacije
@client.command()
@commands.has_permissions(administrator=True)
async def kick(ctx, member:discord.Member = None):
    if not member:
        await ctx.send("Molim izaberi korisnika")
        return
    await member.kick()
    await ctx.send(f"{member.mention} je kikovan")
@kick.error
async def kick_error(ctx, error):
    if isinstance(error, commands.CheckFailure):
        await ctx.send("Nemaš dozovlu da kickuješ korisnike!")
 
@client.command()
@commands.has_permissions(administrator=True)
async def ban(ctx, member:discord.Member = None):
    if not member:
        await ctx.send("Molim izaberi korisnika")
        return
    await member.ban()
    await ctx.send(f"{member.mention} je banovan")
@ban.error
async def kick_error(ctx, error):
    if isinstance(error, commands.CheckFailure):
        await ctx.send("Nemaš dozvolu da banuješ korisnike!")
 
@client.command()
@commands.has_permissions(administrator=True)
async def mute(ctx, member: discord.Member=None):
    if not member:
        await ctx.send("Molim izaberi korisnika")
        return
    role = discord.utils.get(ctx.guild.roles, name="muted")
    await member.add_roles(role)
@mute.error
async def mute_error(ctx, error):
    if isinstance(error, commands.CheckFailure):
        await ctx.send("Nemaš dozvolu da mutuješ korisnike!")
 
 
@client.command()
@commands.has_permissions(administrator=True)
async def unmute(ctx, member: discord.Member=None):
    if not member:
        await ctx.send("Molim izaberi korisnika")
        return
    role = discord.utils.get(ctx.guild.roles, name="muted")
    await member.remove_roles(role)
@mute.error
async def unmute_error(ctx, error):
    if isinstance(error, commands.CheckFailure):
        await ctx.send("Nemaš dozvolu da unmutuješ korisnike!")


client.run("NTI2MTc4MjM3MzQxMTA2MTc3.DwDyLg.v2YmHAvkgrOdKnlrkz3rRywopsE")
